#pragma once
#include "platform/Platform.hpp"
#include <algorithm>
namespace Pine {
inline void normalizeWifiResults(std::vector<WifiNetwork>& networks){
    std::erase_if(networks,[](const auto& n){return n.ssid.empty()||n.ssid.size()>32;});
    std::ranges::sort(networks,[](const auto& a,const auto& b){
        if(a.rssi!=b.rssi)return a.rssi>b.rssi;
        if(a.ssid!=b.ssid)return a.ssid<b.ssid;
        return a.security<b.security;
    });
    std::vector<WifiNetwork> unique;
    for(auto& network:networks){
        if(std::ranges::none_of(unique,[&](const auto& n){return n.ssid==network.ssid&&n.security==network.security;}))
            unique.push_back(std::move(network));
        if(unique.size()==32)break;
    }
    networks=std::move(unique);
}
}
