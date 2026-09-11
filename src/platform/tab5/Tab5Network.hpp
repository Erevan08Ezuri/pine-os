#pragma once
#include "platform/Platform.hpp"
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
class Tab5Network final:public NetworkBackend{
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    std::thread worker_;
    bool stop_{},requested_{},changed_{true},newCredentials_{};
    std::string ssid_,password_,status_{"Not configured"};
    std::atomic_bool connected_{},reconnect_{};
    esp_event_handler_instance_t wifiHandler_{},ipHandler_{};
    static void event(void* arg,esp_event_base_t base,int32_t id,void*){
        auto& self=*static_cast<Tab5Network*>(arg);
        if(base==IP_EVENT&&id==IP_EVENT_STA_GOT_IP)self.connected_=true;
        if(base==WIFI_EVENT&&id==WIFI_EVENT_STA_DISCONNECTED){self.connected_=false;self.reconnect_=true;}
    }
    void status(std::string v){std::lock_guard guard(mutex_);status_=std::move(v);}
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
    void run(){
        bool initialized=false,started=false,timeSync=false;
        for(;;){
            bool requested,change,saveCredentials;std::string ssid,password;
            {
                std::unique_lock guard(mutex_);
                wake_.wait_for(guard,std::chrono::seconds(10),[&]{return stop_||changed_;});
                if(stop_)break;
                requested=requested_;change=changed_;changed_=false;
                saveCredentials=newCredentials_;newCredentials_=false;ssid=ssid_;password=password_;
            }
            if(saveCredentials&&!save(ssid,password)){status("Could not save Wi-Fi settings");continue;}
            if(!requested){
                if(started)esp_wifi_stop();
                started=false;connected_=false;status("Wi-Fi off");continue;
            }
            if(ssid.empty()){status("Enter network name");continue;}
            if(!initialized){
                status("Starting Wi-Fi...");
                if(esp_netif_init()!=ESP_OK){status("Network initialization failed");continue;}
                auto err=esp_event_loop_create_default();
                if(err!=ESP_OK&&err!=ESP_ERR_INVALID_STATE){status("Event initialization failed");continue;}
                if(!esp_netif_create_default_wifi_sta()){status("Network interface failed");return;}
                wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();
                if(esp_wifi_init(&cfg)!=ESP_OK){status("C6 Wi-Fi initialization failed");return;}
                esp_wifi_set_storage(WIFI_STORAGE_RAM);
                esp_event_handler_instance_register(WIFI_EVENT,ESP_EVENT_ANY_ID,event,this,&wifiHandler_);
                esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,event,this,&ipHandler_);
                initialized=true;change=true;
            }
            if(change||!started){
                if(started)esp_wifi_stop();
                connected_=false;started=false;
                wifi_config_t cfg{};
                std::memcpy(cfg.sta.ssid,ssid.data(),ssid.size());
                std::memcpy(cfg.sta.password,password.data(),password.size());
                cfg.sta.threshold.authmode=password.empty()?WIFI_AUTH_OPEN:WIFI_AUTH_WPA2_PSK;
                if(esp_wifi_set_mode(WIFI_MODE_STA)!=ESP_OK||esp_wifi_set_config(WIFI_IF_STA,&cfg)!=ESP_OK||esp_wifi_start()!=ESP_OK){
                    status("Could not start Wi-Fi");continue;
                }
                started=true;reconnect_=true;
            }
            if(reconnect_.exchange(false)&&!connected_){
                status("Connecting...");
                if(esp_wifi_connect()!=ESP_OK){status("Connection failed; retrying");reconnect_=true;}
            }
            if(connected_){
                status(ssid);
                if(!timeSync){
                    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
                    esp_sntp_setservername(0,"pool.ntp.org");esp_sntp_init();timeSync=true;
                }
            }
        }
        if(started)esp_wifi_stop();
        if(initialized){
            esp_event_handler_instance_unregister(WIFI_EVENT,ESP_EVENT_ANY_ID,wifiHandler_);
            esp_event_handler_instance_unregister(IP_EVENT,IP_EVENT_STA_GOT_IP,ipHandler_);
        }
    }
public:
    Tab5Network(){load();worker_=std::thread([this]{run();});}
    ~Tab5Network(){
        {std::lock_guard guard(mutex_);stop_=true;}
        wake_.notify_one();if(worker_.joinable())worker_.join();
    }
    bool wifiEnabled()const override{std::lock_guard guard(mutex_);return requested_;}
    void setWifiEnabled(bool v)override{{std::lock_guard guard(mutex_);requested_=v;changed_=true;}wake_.notify_one();}
    bool connected()const override{return connected_;}
    std::string networkName()const override{std::lock_guard guard(mutex_);return status_;}
    bool configure(const std::string& s,const std::string& p)override{
        if(s.empty()||s.size()>32||p.size()>63||(!p.empty()&&p.size()<8))return false;
        {std::lock_guard guard(mutex_);ssid_=s;password_=p;requested_=true;changed_=true;newCredentials_=true;}
        wake_.notify_one();return true;
    }
};
}
