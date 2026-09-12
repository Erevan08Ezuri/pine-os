#include "platform/tab5/Tab5Bluetooth.hpp"
#include "platform/tab5/Tab5Radio.hpp"
#include "esp_log.h"
extern "C" {
#include "nimble/nimble_port.h"
#include "host/ble_hs.h"
#include "host/ble_gap.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
}
#include <chrono>
#include <cstdio>
#include <cstring>
#include <system_error>

// ESP-Hosted 1.4.0 supplies ll_init but not the deinit hook required by IDF 5.5.1.
// The shared SDIO transport belongs to ESP-Hosted/Wi-Fi, so BLE host teardown
// must leave it running. Controller GAP activity is stopped before this hook.
extern "C" void ble_transport_ll_deinit(void){}

namespace Pine {
namespace {
constexpr auto ScanMilliseconds=10000;
constexpr auto ConnectMilliseconds=10000;
std::string addressId(const ble_addr_t& address){
    char id[24];
    std::snprintf(id,sizeof(id),"%02X:%02X:%02X:%02X:%02X:%02X/%u",
        address.val[5],address.val[4],address.val[3],address.val[2],address.val[1],address.val[0],address.type);
    return id;
}
bool parseAddress(const std::string& id,ble_addr_t& address){
    unsigned bytes[6],type;int consumed{};
    if(std::sscanf(id.c_str(),"%2x:%2x:%2x:%2x:%2x:%2x/%u%n",
        &bytes[5],&bytes[4],&bytes[3],&bytes[2],&bytes[1],&bytes[0],&type,&consumed)!=7||
        static_cast<std::size_t>(consumed)!=id.size()||type>BLE_ADDR_RANDOM_ID)return false;
    for(int i=0;i<6;++i)address.val[i]=static_cast<std::uint8_t>(bytes[i]);
    address.type=static_cast<std::uint8_t>(type);return true;
}
std::string errorMessage(const char* operation,int code){
    ESP_LOGW("PineBLE","%s: %d",operation,code);
    return std::string(operation)+" ("+std::to_string(code)+")";
}
}
Tab5Bluetooth* Tab5Bluetooth::instance_{};
Tab5Bluetooth::Tab5Bluetooth(){
    // One platform owns the host for its entire lifetime.
    instance_=this;
    try{worker_=std::thread([this]{run();});}
    catch(const std::system_error&){state_.view.status="Bluetooth worker could not start";}
}
Tab5Bluetooth::~Tab5Bluetooth(){
    {std::lock_guard lock(mutex_);stopping_=true;state_.enable(false);++generation_;}
    wake_.notify_all();
    if(worker_.joinable())worker_.join();
    // run() stops and joins the host before its callback target is destroyed.
    instance_=nullptr;
}
bool Tab5Bluetooth::enabled()const{std::lock_guard lock(mutex_);return state_.view.enabled;}
BluetoothSnapshot Tab5Bluetooth::snapshot()const{
    std::lock_guard lock(mutex_);return state_.view;
}
void Tab5Bluetooth::setEnabled(bool value){
    {
        std::lock_guard lock(mutex_);
        if(value==state_.view.enabled)return;
        if(value&&!worker_.joinable()){state_.view.status="Bluetooth worker unavailable";return;}
        ++generation_;state_.enable(value);command_=Command::None;
        if(!value)disablePending_=true;
    }
    wake_.notify_all();
}
void Tab5Bluetooth::startScan(){
    {
        std::lock_guard lock(mutex_);
        if(command_!=Command::None||!state_.beginScan())return;
        command_=Command::Scan;
    }
    wake_.notify_one();
}
void Tab5Bluetooth::stopScan(){
    {
        std::lock_guard lock(mutex_);
        if(!state_.view.scanning)return;
        ++generation_;
        if(command_==Command::Scan)command_=Command::None;
        cancelPending_=true;state_.finishScan("Scan stopped");
    }
    wake_.notify_all();
}
bool Tab5Bluetooth::connect(const std::string& id){
    {
        std::lock_guard lock(mutex_);
        if(command_!=Command::None||!state_.beginConnect(id))return false;
        target_=id;command_=Command::Connect;
    }
    wake_.notify_one();return true;
}
bool Tab5Bluetooth::disconnect(const std::string& id){
    {
        std::lock_guard lock(mutex_);
        if(command_!=Command::None||!state_.beginDisconnect(id))return false;
        target_=id;command_=Command::Disconnect;
    }
    wake_.notify_one();return true;
}
void Tab5Bluetooth::fail(const std::string& message,std::uint64_t generation){
    std::lock_guard lock(mutex_);if(generation==generation_)state_.fail(message);
}
void Tab5Bluetooth::onSync(){
    auto* self=instance_;if(!self)return;
    int rc=ble_hs_util_ensure_addr(0);std::uint8_t type{};
    if(!rc)rc=ble_hs_id_infer_auto(0,&type);
    {
        std::lock_guard lock(self->mutex_);
        self->synced_=rc==0;self->ownAddressType_=type;
        if(rc)self->state_.fail(errorMessage("BLE address unavailable",rc));
    }
    self->wake_.notify_all();
}
void Tab5Bluetooth::onReset(int reason){
    auto* self=instance_;if(!self)return;
    {
        std::lock_guard lock(self->mutex_);
        self->synced_=false;self->handle_=BLE_HS_CONN_HANDLE_NONE;
        for(auto& d:self->state_.view.devices)d.connected=false;
        self->connectedId_.clear();self->connectingId_.clear();
        self->state_.fail(errorMessage("BLE radio reset - retry Scan",reason));
    }
    self->wake_.notify_all();
}
int Tab5Bluetooth::onGap(ble_gap_event* event,void* arg){
    auto& self=*static_cast<Tab5Bluetooth*>(arg);
    switch(event->type){
    case BLE_GAP_EVENT_DISC:{
        const auto& report=event->disc;
        ble_hs_adv_fields fields{};
        if(ble_hs_adv_parse_fields(&fields,report.data,report.length_data)!=0)return 0;
        std::string_view name;
        if(fields.name&&fields.name_len)name={reinterpret_cast<const char*>(fields.name),fields.name_len};
        const bool connectable=report.event_type==BLE_HCI_ADV_RPT_EVTYPE_ADV_IND||
            report.event_type==BLE_HCI_ADV_RPT_EVTYPE_DIR_IND;
        std::lock_guard lock(self.mutex_);
        self.state_.observe(addressId(report.addr),name,report.rssi,connectable);
        return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:{
        std::lock_guard lock(self.mutex_);
        if(self.state_.view.scanning)self.state_.finishScan(
            event->disc_complete.reason==0?"Scan complete":"Scan ended ("+std::to_string(event->disc_complete.reason)+")");
        return 0;
    }
    case BLE_GAP_EVENT_CONNECT:{
        bool reject=false;
        {
            std::lock_guard lock(self.mutex_);
            if(event->connect.status==0){
                reject=self.stopping_||self.connectGeneration_!=self.generation_||
                    !self.state_.connected(self.connectingId_);
                // Track even a late connection until its termination is confirmed.
                self.handle_=event->connect.conn_handle;self.connectedId_=self.connectingId_;
            }else if(self.connectGeneration_==self.generation_){
                self.state_.fail(errorMessage("Connection failed",event->connect.status));
            }
            self.connectingId_.clear();
        }
        if(reject)ble_gap_terminate(event->connect.conn_handle,BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    case BLE_GAP_EVENT_DISCONNECT:{
        std::lock_guard lock(self.mutex_);
        if(event->disconnect.conn.conn_handle==self.handle_){
            self.state_.disconnected(self.connectedId_);
            self.handle_=BLE_HS_CONN_HANDLE_NONE;self.connectedId_.clear();
        }
        return 0;
    }
    case BLE_GAP_EVENT_REPEAT_PAIRING:
        return BLE_GAP_REPEAT_PAIRING_IGNORE;
    default:return 0;
    }
}
bool Tab5Bluetooth::initialize(std::uint64_t generation){
    if(!initialized_){
        {
            std::lock_guard radioLock(tab5RadioInitMutex());
            // Preflight avoids ESP-Hosted's fatal ESP_ERROR_CHECK when the C6 is
            // absent. The same serialized preflight is used for Wi-Fi startup.
            auto rc=transport_drv_reconfigure();
            if(rc!=ESP_OK){fail("C6 unavailable - check radio firmware",generation);return false;}
            rc=nimble_port_init();
            if(rc!=ESP_OK){fail(errorMessage("BLE initialization failed",rc),generation);return false;}
        }
        ble_hs_cfg.sync_cb=onSync;ble_hs_cfg.reset_cb=onReset;
        // This stage creates a BLE link only; no pairing, bonds or protected
        // application data exchange. Do not silently accept pairing requests.
        try{host_=std::thread([]{nimble_port_run();});}
        catch(const std::system_error&){nimble_port_deinit();fail("BLE host task could not start",generation);return false;}
        initialized_=true;
    }
    std::unique_lock lock(mutex_);
    wake_.wait_for(lock,std::chrono::seconds(12),[this]{return synced_||stopping_||disablePending_||cancelPending_;});
    if(!synced_){
        if(!stopping_&&!disablePending_&&!cancelPending_&&generation==generation_)state_.fail("C6 BLE not ready - check radio firmware");
        return false;
    }
    return true;
}
void Tab5Bluetooth::cancelOperations(){
    if(!initialized_)return;
    if(ble_gap_disc_active())ble_gap_disc_cancel();
    if(ble_gap_conn_active())ble_gap_conn_cancel();
    std::uint16_t handle;
    {std::lock_guard lock(mutex_);handle=handle_;}
    if(handle!=BLE_HS_CONN_HANDLE_NONE)ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
}
void Tab5Bluetooth::run(){
    for(;;){
        Command command;std::string target;std::uint64_t generation;bool disable,cancel;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock,[this]{return stopping_||disablePending_||cancelPending_||command_!=Command::None;});
            if(stopping_)break;
            command=command_;command_=Command::None;target=target_;generation=generation_;
            disable=disablePending_;disablePending_=false;cancel=cancelPending_;cancelPending_=false;
        }
        if(disable)cancelOperations();
        if(cancel&&initialized_&&ble_gap_disc_active())ble_gap_disc_cancel();
        if(command==Command::None)continue;
        if(!initialize(generation))continue;
        std::uint8_t type;std::uint16_t handle;
        {
            std::lock_guard lock(mutex_);
            if(stopping_||!state_.view.enabled||generation!=generation_)continue;
            type=ownAddressType_;handle=handle_;
        }
        int rc=0;
        if(command==Command::Scan){
            ble_gap_disc_params params{};
            params.itvl=160;params.window=80; // 100 ms interval, 50 ms window.
            params.passive=0;params.filter_duplicates=0; // Keep scan response names and fresh RSSI.
            rc=ble_gap_disc(type,ScanMilliseconds,&params,onGap,this);
            if(!rc){std::lock_guard lock(mutex_);if(state_.view.scanning)state_.view.status="Scanning nearby BLE devices...";}
        }else if(command==Command::Connect){
            if(ble_gap_disc_active())rc=ble_gap_disc_cancel();
            if(rc==BLE_HS_EALREADY)rc=0;
            ble_addr_t address{};
            if(!parseAddress(target,address))rc=BLE_HS_EINVAL;
            if(!rc){
                {std::lock_guard lock(mutex_);connectingId_=target;connectGeneration_=generation;}
                rc=ble_gap_connect(type,&address,ConnectMilliseconds,nullptr,onGap,this);
            }
        }else if(command==Command::Disconnect){
            rc=handle==BLE_HS_CONN_HANDLE_NONE?BLE_HS_ENOTCONN:ble_gap_terminate(handle,BLE_ERR_REM_USER_CONN_TERM);
        }
        if(rc){
            std::lock_guard lock(mutex_);
            if(generation==generation_)state_.fail(errorMessage(command==Command::Scan?"Scan failed":"BLE request failed",rc));
        }
    }
    if(initialized_){
        cancelOperations();
        const auto rc=nimble_port_stop();
        if(rc)ESP_LOGE("PineBLE","Host stop failed: %d",rc);
        // Never free callback state while the host is still running.
        if(host_.joinable())host_.join();
        nimble_port_deinit();
    }
}
}
