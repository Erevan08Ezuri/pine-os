#pragma once
#include "platform/Platform.hpp"
namespace Pine {
class DesktopNetwork final:public NetworkBackend {
public:
    bool wifiEnabled()const override{return enabled_;}
    void setWifiEnabled(bool v)override{enabled_=v;if(!v){scan_={false,"Wi-Fi off",{}};}}
    bool connected()const override{return enabled_;}
    std::string networkName()const override{return enabled_?name_:"Not connected";}
    void startScan()override{
        if(!enabled_){scan_={false,"Turn on Wi-Fi to scan",{}};return;}
        scan_={false,"Simulated networks",{{"Pine Home","WPA2",-38,true},{"Workshop","WPA2",-57,true},{"Guest Wi-Fi","Open",-68,true}}};
    }
    WifiScanSnapshot scanSnapshot()const override{return scan_;}
    bool configure(const std::string& s,const std::string& p)override{
        if(s.empty()||s.size()>32||p.size()>63||(!p.empty()&&p.size()<8))return false;
        name_=s;enabled_=true;return true;
    }
private:
    bool enabled_{true};std::string name_{"Desktop Host Network"};WifiScanSnapshot scan_;
};
}
