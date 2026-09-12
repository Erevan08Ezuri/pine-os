#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include <algorithm>
namespace Pine {
void BluetoothApp::render(UiContext& ui){ui.renderBluetooth();}
void Shell::renderBluetooth(){
    const auto state=bluetooth_.snapshot();
    auto fit=[](std::string text,float width,float scale){
        while(!text.empty()&&textWidth(text,scale)>width){
            auto end=text.size()-1;
            while(end>0&&(static_cast<unsigned char>(text[end])&0xc0)==0x80)--end;
            text.resize(end);
        }
        return text;
    };
    label(45,105,"BLUETOOTH",5,Theme::Gold);
    if(button({485,115,180,55},state.enabled?"ON":"OFF",state.enabled)){
        bluetooth_.setEnabled(!state.enabled);bluetoothPage_=0;persist();
    }
    sublabel(48,185,state.adapter);
    label(48,225,fit(state.status,620,2.4f),2.4f,Theme::Gold);
    if(state.enabled&&!state.busy&&button({45,275,220,60},state.scanning?"STOP SCAN":"SCAN",true)){
        if(state.scanning)bluetooth_.stopScan();else{bluetooth_.startScan();bluetoothPage_=0;}
    }
#ifdef PINE_TAB5
    sublabel(48,355,"BLE LINKS - AUDIO AND FILE TRANSFER NOT YET AVAILABLE");
#else
    sublabel(48,355,"SIMULATED DEVICES - NO PHYSICAL RADIO CONNECTION");
#endif
    constexpr std::size_t pageSize=6;
    const auto pages=std::max<std::size_t>(1,(state.devices.size()+pageSize-1)/pageSize);
    bluetoothPage_=std::min(bluetoothPage_,pages-1);
    if(state.devices.empty())sublabel(48,420,state.scanning?"LOOKING FOR BLE ADVERTISEMENTS...":"NO DEVICES - TURN ON BLUETOOTH AND SCAN");
    float y=400;
    const auto end=std::min(state.devices.size(),(bluetoothPage_+1)*pageSize);
    for(auto i=bluetoothPage_*pageSize;i<end;++i){
        const auto& d=state.devices[i];
        panel({40,y,640,100});
        label(60,y+13,fit(d.name,420,2.8f),2.8f);
        sublabel(60,y+47,fit(d.id,420,2));
        std::string status=d.connecting?"CONNECTING":d.disconnecting?"DISCONNECTING":
            d.connected?"CONNECTED":d.connectable?"AVAILABLE":"BROADCAST ONLY";
        if(d.rssi!=127)status+=" / "+std::to_string(d.rssi)+" dBm";
        label(60,y+74,status,1.8f,Theme::Muted);
        if(state.enabled&&!state.busy&&(d.connected||d.connectable)&&
           button({500,y+25,155,52},d.connected?"DISCONNECT":"CONNECT",d.connected)){
            const bool accepted=d.connected?bluetooth_.disconnect(d.id):bluetooth_.connect(d.id);
            if(!accepted)toast("REQUEST NOT ACCEPTED - CHECK BLUETOOTH STATUS");
        }
        y+=110;
    }
    if(pages>1){
        if(bluetoothPage_>0&&button({45,1090,150,50},"PREVIOUS"))--bluetoothPage_;
        label(285,1105,std::to_string(bluetoothPage_+1)+" / "+std::to_string(pages),2.5f);
        if(bluetoothPage_+1<pages&&button({510,1090,150,50},"NEXT"))++bluetoothPage_;
    }
}
}
