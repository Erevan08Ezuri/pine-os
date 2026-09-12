// Execute the real asynchronous Tab5 adapter against a controlled HCI/GAP shim.
// This covers host state/lifetimes, not real RF or the ESP-Hosted wire protocol.
#include "FakeNimble.hpp"
#include "platform/tab5/Tab5Bluetooth.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
ble_hs_cfg_t ble_hs_cfg;
namespace {
std::mutex mutex;std::condition_variable wake;bool stopped{};
std::atomic_bool scanning{},connecting{};
std::atomic_int transportResult{},terminationCount{};
ble_gap_event_fn* callback{};void* context{};
ble_addr_t lastAddress{};
void emit(ble_gap_event event){ble_gap_event_fn* cb;void* ctx;{std::lock_guard lock(mutex);cb=callback;ctx=context;}if(cb)cb(&event,ctx);}
void report(const char* name){
    ble_gap_event event{};event.type=BLE_GAP_EVENT_DISC;event.disc.addr.type=1;event.disc.addr.val[0]=0xab;
    event.disc.rssi=-42;event.disc.data=reinterpret_cast<const std::uint8_t*>(name);event.disc.length_data=std::strlen(name);emit(event);
}
template<class F>void until(F condition){
    auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
    while(!condition()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));
    if(!condition())throw std::runtime_error("Timed out waiting for BLE adapter state");
}
#define CHECK(x) do{if(!(x))throw std::runtime_error("Check failed: " #x);}while(false)
}
extern "C" {
esp_err_t transport_drv_reconfigure(){return transportResult;}
esp_err_t nimble_port_init(){std::lock_guard lock(mutex);stopped=false;return 0;}
esp_err_t nimble_port_deinit(){return 0;}
int nimble_port_stop(){{std::lock_guard lock(mutex);stopped=true;}wake.notify_all();return 0;}
void nimble_port_run(){ble_hs_cfg.sync_cb();std::unique_lock lock(mutex);wake.wait(lock,[]{return stopped;});}
int ble_hs_util_ensure_addr(int){return 0;}
int ble_hs_id_infer_auto(int,std::uint8_t* type){*type=0;return 0;}
int ble_hs_adv_parse_fields(ble_hs_adv_fields* fields,const std::uint8_t* data,std::uint8_t size){fields->name=data;fields->name_len=size;return 0;}
int ble_gap_disc_active(){return scanning;}
int ble_gap_disc_cancel(){scanning=false;return 0;}
int ble_gap_conn_active(){return connecting;}
int ble_gap_conn_cancel(){connecting=false;ble_gap_event event{};event.type=BLE_GAP_EVENT_CONNECT;event.connect.status=19;emit(event);return 0;}
int ble_gap_disc(std::uint8_t,int duration,const ble_gap_disc_params*,ble_gap_event_fn* cb,void* ctx){
    CHECK(duration==10000);{std::lock_guard lock(mutex);callback=cb;context=ctx;}scanning=true;return 0;
}
int ble_gap_connect(std::uint8_t,const ble_addr_t* address,int duration,const void*,ble_gap_event_fn* cb,void* ctx){
    CHECK(duration==10000);{std::lock_guard lock(mutex);lastAddress=*address;callback=cb;context=ctx;}connecting=true;return 0;
}
int ble_gap_terminate(std::uint16_t handle,int){++terminationCount;ble_gap_event event{};event.type=BLE_GAP_EVENT_DISCONNECT;event.disconnect.conn.conn_handle=handle;emit(event);return 0;}
}
int main(){
    try{
        transportResult=-1;
        {Pine::Tab5Bluetooth ble;ble.setEnabled(true);ble.startScan();until([&]{return !ble.snapshot().scanning;});CHECK(ble.snapshot().status.find("C6 unavailable")!=std::string::npos);}
        transportResult=0;
        {
            Pine::Tab5Bluetooth ble;ble.setEnabled(true);ble.startScan();until([]{return scanning.load();});
            report("Sensor");report("Sensor updated");CHECK(ble.devices().size()==1);
            const auto id=ble.devices()[0].id;CHECK(id=="00:00:00:00:00:AB/1");CHECK(ble.devices()[0].name=="Sensor updated");
            ble.stopScan();until([]{return !scanning.load();});CHECK(!ble.snapshot().scanning);
            CHECK(ble.connect(id));until([]{return connecting.load();});CHECK(!ble.devices()[0].connected);
            {std::lock_guard lock(mutex);CHECK(lastAddress.type==1&&lastAddress.val[0]==0xab);}
            connecting=false;ble_gap_event connection{};connection.type=BLE_GAP_EVENT_CONNECT;connection.connect.conn_handle=7;emit(connection);
            CHECK(ble.devices()[0].connected);CHECK(ble.disconnect(id));until([&]{return !ble.snapshot().busy;});CHECK(!ble.devices()[0].connected);
            CHECK(ble.connect(id));until([]{return connecting.load();});
            // An Off action wins over a late controller success.
            ble.setEnabled(false);connecting=false;emit(connection);
            until([]{return terminationCount.load()>=2;});CHECK(!ble.enabled()&&ble.devices().empty());
            ble.setEnabled(true);ble.startScan();until([]{return scanning.load();});report("After reset");CHECK(ble.devices().size()==1);
            ble_hs_cfg.reset_cb(123);CHECK(!ble.snapshot().scanning&&!ble.snapshot().busy);
            CHECK(ble.snapshot().status.find("reset")!=std::string::npos);
        } // Also exercises stopping/joining host and worker with callbacks alive.
        std::cout<<"Tab5 BLE event simulation passed.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
