#pragma once
#include "platform/Platform.hpp"
#include "core/Utf8.hpp"
#include <algorithm>
#include <string_view>

namespace Pine {
// Hardware-independent central state. The adapter serializes access with its mutex.
class BleCentralState {
public:
    static constexpr std::size_t MaxDevices=32;
    BluetoothSnapshot view{false,false,false,"TAB5 BLUETOOTH LE","Bluetooth off",{}};
    void enable(bool value){
        view.enabled=value;view.scanning=false;view.busy=false;view.devices.clear();
        view.status=value?"Ready - tap Scan":"Bluetooth off";
    }
    bool beginScan(){
        if(!view.enabled||view.scanning||view.busy)return false;
        std::erase_if(view.devices,[](const auto& d){return !d.connected;});
        view.scanning=true;view.status="Starting scan...";return true;
    }
    void finishScan(const std::string& message){view.scanning=false;if(!view.busy&&view.enabled)view.status=message;}
    static std::string displayName(std::string_view name){
        std::string result;
        for(std::size_t p=0;p<name.size()&&result.size()<64;){
            const auto start=p;const auto c=nextUtf8(name,p);
            if(c<32||c==127)continue;
            if(c==0xfffd){result+='?';continue;}
            if(result.size()+p-start>64)break;
            result.append(name.substr(start,p-start));
        }
        return result;
    }
    void observe(const std::string& id,std::string_view name,int rssi,bool connectable){
        if(!view.enabled||!view.scanning)return;
        auto it=find(id);const auto clean=displayName(name);
        if(it==view.devices.end()){
            if(view.devices.size()>=MaxDevices)return;
            view.devices.push_back({id,clean.empty()?"Unnamed BLE device":clean,"BLE",false,rssi,connectable});
        }else{
            if(!clean.empty())it->name=clean;
            it->rssi=rssi;it->connectable=it->connectable||connectable;
        }
    }
    bool beginConnect(const std::string& id){
        auto it=find(id);
        if(!view.enabled||view.busy||it==view.devices.end()||!it->connectable||it->connected)return false;
        if(std::ranges::any_of(view.devices,[](const auto& d){return d.connected;})){
            view.status="Disconnect the current device first";return false;
        }
        view.scanning=false;view.busy=true;it->connecting=true;view.status="Connecting...";return true;
    }
    bool connected(const std::string& id){
        auto it=find(id);
        if(!view.enabled||it==view.devices.end()||!it->connecting)return false;
        it->connecting=false;it->connected=true;view.busy=false;view.status="BLE link connected";return true;
    }
    bool beginDisconnect(const std::string& id){
        auto it=find(id);
        if(view.busy||it==view.devices.end()||!it->connected)return false;
        view.busy=true;it->disconnecting=true;view.status="Disconnecting...";return true;
    }
    void disconnected(const std::string& id){
        auto it=find(id);if(it==view.devices.end())return;
        const bool pending=it->disconnecting||it->connecting;
        it->connected=it->connecting=it->disconnecting=false;
        if(pending)view.busy=false;
        if(view.enabled&&!view.scanning&&!view.busy)view.status="Disconnected";
    }
    void fail(const std::string& message){
        view.scanning=false;view.busy=false;
        for(auto& d:view.devices)d.connecting=d.disconnecting=false;
        if(view.enabled)view.status=message;
    }
private:
    auto find(const std::string& id)->std::vector<BluetoothDevice>::iterator{
        return std::ranges::find_if(view.devices,[&](const auto& d){return d.id==id;});
    }
};
}
