#pragma once
#include "platform/Platform.hpp"
#include "platform/WifiScanResults.hpp"
#include "platform/tab5/Tab5Radio.hpp"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs.h"
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
namespace Pine {
class Tab5Network final:public NetworkBackend {
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stop_{},requested_{},changed_{true},newCredentials_{},scanRequested_{};
    std::string ssid_,password_,status_{"Not configured"};
    WifiScanSnapshot scan_;
    std::atomic_bool connected_{},reconnect_{},scanDone_{},workerDone_{};
    std::uint64_t generation_{};
    esp_event_handler_instance_t wifiHandler_{},ipHandler_{};
    static void event(void* arg,esp_event_base_t base,int32_t id,void*){
        auto& self=*static_cast<Tab5Network*>(arg);
        if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP)self.connected_=true;
        if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){self.connected_=false;self.reconnect_=true;}
        if(base==WIFI_EVENT&&id==WIFI_EVENT_SCAN_DONE)self.scanDone_=true;
        self.wake_.notify_one();
    }
    void status(std::string v){std::lock_guard guard(mutex_);status_=std::move(v);}
    void scanError(const std::string& message){std::lock_guard guard(mutex_);scan_.scanning=false;scan_.status=message;}
    void load(){
        nvs_handle_t h;if(nvs_open("pine_wifi",NVS_READONLY,&h)!=ESP_OK)return;
        char s[33]{},p[65]{};size_t sl=sizeof(s),pl=sizeof(p);
        if(nvs_get_str(h,"ssid",s,&sl)==ESP_OK&&nvs_get_str(h,"password",p,&pl)==ESP_OK){ssid_=s;password_=p;}
        nvs_close(h);
    }
    bool save(const std::string& s,const std::string& p){
        nvs_handle_t h;if(nvs_open("pine_wifi",NVS_READWRITE,&h)!=ESP_OK)return false;
        bool ok=nvs_set_str(h,"ssid",s.c_str())==ESP_OK&&nvs_set_str(h,"password",p.c_str())==ESP_OK&&nvs_commit(h)==ESP_OK;
        nvs_close(h);return ok;
    }
    void collectScan(std::uint64_t generation){
        std::uint16_t count=64;std::vector<wifi_ap_record_t> records(count);
        const auto rc=esp_wifi_scan_get_ap_records(&count,records.data());
        esp_wifi_clear_ap_list();
        if(rc!=ESP_OK){scanError("Could not read scan results - retry");return;}
        std::vector<WifiNetwork> networks;
        for(unsigned i=0;i<count;++i){
            const auto& ap=records[i];std::string security;bool supported=true;
            switch(ap.authmode){
            case WIFI_AUTH_OPEN:security="Open";break;
            case WIFI_AUTH_WPA_PSK:security="WPA";break;
            case WIFI_AUTH_WPA2_PSK:security="WPA2";break;
            case WIFI_AUTH_WPA_WPA2_PSK:security="WPA/WPA2";break;
            case WIFI_AUTH_WPA2_WPA3_PSK:security="WPA2/WPA3";break;
            default:security="Unsupported security";supported=false;break;
            }
            networks.push_back({std::string(reinterpret_cast<const char*>(ap.ssid),strnlen(reinterpret_cast<const char*>(ap.ssid),32)),security,ap.rssi,supported});
        }
        normalizeWifiResults(networks);
        std::lock_guard guard(mutex_);
        if(!requested_||generation!=generation_)return;
        scan_={false,networks.empty()?"No visible networks found":"Scan complete",std::move(networks)};
    }
    void run(){
        bool initialized=false,started=false,timeSync=false,scanning=false;
        std::uint64_t activeScanGeneration{};
        auto scanDeadline=std::chrono::steady_clock::now(),retryAfter=scanDeadline;
        for(;;){
            bool requested,change,saveCredentials,scanRequest;std::string ssid,password;std::uint64_t generation;
            {
                std::unique_lock guard(mutex_);
                wake_.wait_for(guard,std::chrono::milliseconds(250),[&]{return stop_||changed_||scanRequested_||scanDone_;});
                if(stop_)break;
                requested=requested_;change=changed_;changed_=false;generation=generation_;
                saveCredentials=newCredentials_;newCredentials_=false;
                scanRequest=scanRequested_;scanRequested_=false;ssid=ssid_;password=password_;
            }
            if(saveCredentials&&!save(ssid,password)){status("Could not save Wi-Fi settings");scanError("Could not save Wi-Fi settings");continue;}
            if(!requested){
                if(scanning){esp_wifi_scan_stop();esp_wifi_clear_ap_list();}
                if(started)esp_wifi_stop();
                scanning=started=false;scanDone_=false;connected_=false;reconnect_=false;
                status("Wi-Fi off");continue;
            }
            if(ssid.empty()&&!scanRequest&&!scanning&&!initialized){status("Scan and select a network");continue;}
            if(!initialized){
                status("Starting Wi-Fi...");
                if(esp_netif_init()!=ESP_OK){status("Network initialization failed");scanError("Network initialization failed");continue;}
                const auto err=esp_event_loop_create_default();
                if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE){status("Event initialization failed");scanError("Event initialization failed");continue;}
                if(!esp_netif_create_default_wifi_sta()){status("Network interface failed");scanError("Network interface failed");return;}
                wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
                {
                    std::lock_guard radioLock(tab5RadioInitMutex());
                    if(transport_drv_reconfigure()!=ESP_OK||esp_wifi_init(&cfg)!=ESP_OK){status("C6 Wi-Fi initialization failed");scanError("C6 unavailable - restart device");return;}
                }
                if(esp_wifi_set_storage(WIFI_STORAGE_RAM)!=ESP_OK||
                   esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,this,&wifiHandler_)!=ESP_OK||
                   esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,this,&ipHandler_)!=ESP_OK){
                    status("Wi-Fi setup failed");scanError("Wi-Fi setup failed");return;
                }
                initialized=true;change=true;
            }
            {std::lock_guard guard(mutex_);if(generation!=generation_)continue;}
            if(change||!started){
                if(scanning){esp_wifi_scan_stop();esp_wifi_clear_ap_list();scanning=false;scanDone_=false;scanError("Scan cancelled");}
                if(started)esp_wifi_stop();
                connected_=false;started=false;reconnect_=false;
                wifi_config_t cfg{};
                std::memcpy(cfg.sta.ssid,ssid.data(),ssid.size());std::memcpy(cfg.sta.password,password.data(),password.size());
                cfg.sta.threshold.authmode=password.empty()?WIFI_AUTH_OPEN:WIFI_AUTH_WPA_PSK;
                if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||esp_wifi_set_config(WIFI_IF_STA,&cfg)!=ESP_OK||esp_wifi_start()!=ESP_OK){
                    status("Could not start Wi-Fi");scanError("Could not start Wi-Fi");continue;
                }
                started=true;reconnect_=!ssid.empty();
            }
            if(scanRequest&&!scanning){
                wifi_scan_config_t cfg{};cfg.show_hidden=false;cfg.scan_type=WIFI_SCAN_TYPE_ACTIVE;
                cfg.scan_time.active.min=40;cfg.scan_time.active.max=120;
                scanDone_=false;
                const auto rc=esp_wifi_scan_start(&cfg,false);
                if(rc==ESP_OK){scanning=true;activeScanGeneration=generation;scanDeadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);}
                else scanError("Radio busy connecting - retry Scan");
            }
            if(scanning&&scanDone_.exchange(false)){scanning=false;collectScan(activeScanGeneration);}
            if(scanning&&std::chrono::steady_clock::now()>scanDeadline){
                esp_wifi_scan_stop();esp_wifi_clear_ap_list();scanning=false;scanDone_=false;scanError("Scan timed out - retry");
            }
            if(!scanning)scanDone_=false;
            if(!scanning&&!ssid.empty()&&!connected_&&reconnect_&&std::chrono::steady_clock::now()>=retryAfter){
                reconnect_=false;status("Connecting...");
                if(esp_wifi_connect()!=ESP_OK){status("Connection failed; retrying");reconnect_=true;}
                retryAfter=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            }
            if(connected_){
                status(ssid);
                if(!timeSync){esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();timeSync=true;}
            }
        }
        if(scanning){esp_wifi_scan_stop();esp_wifi_clear_ap_list();}
        if(started)esp_wifi_stop();
    }
public:
    Tab5Network(){load();worker_=std::thread([this]{
        try{run();}catch(...){status("Wi-Fi worker stopped - restart device");scanError("Wi-Fi worker stopped - restart device");}
        workerDone_=true;
    });}
    ~Tab5Network(){
        {std::lock_guard guard(mutex_);stop_=true;}
        wake_.notify_one();if(worker_.joinable())worker_.join();
        if(wifiHandler_)esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiHandler_);
        if(ipHandler_)esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,ipHandler_);
    }
    bool wifiEnabled()const override{std::lock_guard guard(mutex_);return requested_;}
    void setWifiEnabled(bool v)override{
        {std::lock_guard guard(mutex_);requested_=v;changed_=true;++generation_;if(!v){scanRequested_=false;scan_={false,"Wi-Fi off",{}};}}
        wake_.notify_one();
    }
    bool connected()const override{return connected_;}
    std::string networkName()const override{std::lock_guard guard(mutex_);return status_;}
    void startScan()override{
        {
            std::lock_guard guard(mutex_);
            if(workerDone_){scan_.scanning=false;scan_.status="Wi-Fi unavailable - restart device";return;}
            if(!requested_){scan_.status="Turn on Wi-Fi to scan";return;}
            if(scan_.scanning)return;
            scan_={true,"Scanning nearby networks...",{}};scanRequested_=true;
        }
        wake_.notify_one();
    }
    WifiScanSnapshot scanSnapshot()const override{std::lock_guard guard(mutex_);return scan_;}
    bool configure(const std::string& s,const std::string& p)override{
        if(workerDone_||s.empty()||s.size()>32||s.find('\0')!=std::string::npos||p.size()>63||(!p.empty()&&p.size()<8))return false;
        {std::lock_guard guard(mutex_);ssid_=s;password_=p;requested_=true;changed_=true;newCredentials_=true;++generation_;}
        wake_.notify_one();return true;
    }
};
}
