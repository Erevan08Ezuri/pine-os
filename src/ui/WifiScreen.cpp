#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include "platform/BleCentralState.hpp"
#include <algorithm>
namespace Pine {
void Shell::openWifiScreen(){
    if(pinScreenVisible())return;
    dismissInput();wifiScreen_=true;wifiPage_=0;
    if(network_.wifiEnabled())network_.startScan();
}
void Shell::selectWifiNetwork(const WifiNetwork& network){
    if(!network.connectable){toast("THIS NETWORK SECURITY IS NOT SUPPORTED YET");return;}
    if(network.security=="Open"){
        if(network_.configure(network.ssid,"")){persist();toast("CONNECTING TO "+network.ssid);}
    }else openTextPrompt("WI-FI PASSWORD","",[this,ssid=network.ssid](const std::string& password){
        if(password.size()<8){toast("PASSWORD MUST BE 8-63 CHARACTERS");return;}
        if(network_.configure(ssid,password)){persist();toast("CONNECTING TO "+ssid);}
        else toast("INVALID NETWORK DETAILS");
    },InputType::Password,63);
}
void Shell::renderWifiScreen(){
    label(45,105,"WI-FI NETWORKS",4.8f,Theme::Gold);
    if(button({510,115,155,55},network_.wifiEnabled()?"ON":"OFF",network_.wifiEnabled())){
        network_.setWifiEnabled(!network_.wifiEnabled());persist();
    }
    sublabel(48,183,network_.networkName().substr(0,45));
    const auto scan=network_.scanSnapshot();
    sublabel(48,220,scan.status);
#ifndef PINE_TAB5
    sublabel(48,250,"SIMULATED NETWORK LIST");
#endif
    if(network_.wifiEnabled()&&!scan.scanning&&button({45,285,210,60},"SCAN",true)){network_.startScan();wifiPage_=0;}
    if(button({320,285,345,60},"HIDDEN NETWORK")){
        openTextPrompt("HIDDEN NETWORK NAME","",[this](const std::string& ssid){
            if(ssid.empty())return;
            openTextPrompt("PASSWORD (EMPTY FOR OPEN)","",[this,ssid](const std::string& password){
                if(network_.configure(ssid,password)){persist();toast("CONNECTING TO "+ssid);}
                else toast("INVALID NETWORK DETAILS");
            },InputType::Password,63);
        },InputType::Text,32);
    }
    constexpr std::size_t pageSize=6;
    const auto pages=std::max<std::size_t>(1,(scan.networks.size()+pageSize-1)/pageSize);
    wifiPage_=std::min(wifiPage_,pages-1);
    if(scan.networks.empty())sublabel(48,410,scan.scanning?"LOOKING FOR NEARBY NETWORKS...":"TAP SCAN TO FIND NETWORKS");
    const auto end=std::min(scan.networks.size(),(wifiPage_+1)*pageSize);
    float y=395;
    for(auto i=wifiPage_*pageSize;i<end;++i){
        const auto& network=scan.networks[i];panel({40,y,640,100});
        auto name=BleCentralState::displayName(network.ssid);
        while(!name.empty()&&textWidth(name,2.8f)>420){auto p=name.size()-1;while(p>0&&(static_cast<unsigned char>(name[p])&0xc0)==0x80)--p;name.resize(p);}
        label(60,y+17,name.empty()?"Unnamed network":name,2.8f);
        sublabel(60,y+58,network.security+" / "+std::to_string(network.rssi)+" dBm");
        if(network_.wifiEnabled()&&network.connectable&&button({510,y+23,145,52},"JOIN",true))selectWifiNetwork(network);
        y+=110;
    }
    if(pages>1){
        if(wifiPage_>0&&button({45,1090,150,50},"PREVIOUS"))--wifiPage_;
        label(285,1105,std::to_string(wifiPage_+1)+" / "+std::to_string(pages),2.5f);
        if(wifiPage_+1<pages&&button({510,1090,150,50},"NEXT"))++wifiPage_;
    }
}
}
