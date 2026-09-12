#include "platform/BleCentralState.hpp"
#include "platform/desktop/DesktopBluetooth.hpp"
#include <iostream>
#include <stdexcept>

#define CHECK(x) do{if(!(x))throw std::runtime_error("Check failed at line "+std::to_string(__LINE__)+": " #x);}while(false)
using namespace Pine;
int main(){
    try{
        BleCentralState state;
        CHECK(!state.beginScan());CHECK(!state.beginConnect("missing"));
        state.enable(true);CHECK(state.beginScan());CHECK(!state.beginScan());
        state.observe("01/public","",-70,true);
        state.observe("01/public","Pine sensor",-45,false); // Scan response updates existing advertisement.
        CHECK(state.view.devices.size()==1);CHECK(state.view.devices[0].name=="Pine sensor");
        CHECK(state.view.devices[0].rssi==-45);CHECK(state.view.devices[0].connectable);
        state.observe("01/random","Other",-60,false); // Same bytes, different address type.
        CHECK(state.view.devices.size()==2);CHECK(!state.beginConnect("01/random"));
        CHECK(state.beginConnect("01/public"));CHECK(state.view.busy&&!state.view.scanning);
        CHECK(!state.view.devices[0].connected&&state.view.devices[0].connecting);
        CHECK(!state.beginScan());CHECK(!state.connected("wrong"));
        CHECK(state.connected("01/public"));CHECK(!state.view.busy);
        CHECK(!state.beginConnect("01/public"));
        CHECK(state.beginScan());CHECK(state.view.devices.size()==1);
        CHECK(state.view.devices[0].connected); // Rescan preserves the real connection.
        for(int i=0;i<1000;++i)state.observe(std::to_string(i),"Sensor",-50,true);
        CHECK(state.view.devices.size()==BleCentralState::MaxDevices);
        state.observe("01/public","Updated",-30,true);
        CHECK(state.view.devices[0].rssi==-30); // Existing rows update at capacity.
        CHECK(!state.beginConnect("0")); // One central connection at a time.
        state.finishScan("Scan complete");
        CHECK(state.beginDisconnect("01/public"));CHECK(state.view.devices[0].connected);
        state.disconnected("unrelated");CHECK(state.view.busy);
        state.disconnected("01/public");CHECK(!state.view.busy&&!state.view.devices[0].connected);
        CHECK(state.beginConnect("0"));state.fail("Timeout");
        CHECK(!state.view.busy);CHECK(!state.view.devices[1].connecting);
        CHECK(!state.connected("0")); // Late success after a failed request is rejected.
        CHECK(state.beginConnect("0"));state.enable(false);
        CHECK(!state.connected("0"));CHECK(state.view.devices.empty());
        state.observe("late","Stale advertisement",-10,true);CHECK(state.view.devices.empty());
        state.enable(true);CHECK(state.beginScan());state.finishScan("Scan stopped");
        state.observe("late","Stale advertisement",-10,true);CHECK(state.view.devices.empty());
        const auto name=BleCentralState::displayName(std::string("a\0b\nc\x7f",6)+"\xff");
        CHECK(name=="abc?");
        std::string unicode;for(int i=0;i<50;++i)unicode+="é";
        const auto bounded=BleCentralState::displayName(unicode);
        CHECK(bounded.size()==64);std::size_t pos=0;
        while(pos<bounded.size())CHECK(nextUtf8(bounded,pos)==0xe9);
        DesktopBluetooth desktop;desktop.startScan();CHECK(desktop.connect("pine-buds"));
        auto snapshot=desktop.snapshot();desktop.startScan();CHECK(desktop.devices()[0].connected);
        desktop.setEnabled(false);CHECK(!desktop.devices()[0].connected);
        CHECK(snapshot.devices[0].connected); // Previously returned state is an immutable copy.
        CHECK(!desktop.connect("pine-buds"));
        std::cout<<"Bluetooth regression checks passed (discovery, bounds, lifecycle, failures, UTF-8, snapshots).\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
