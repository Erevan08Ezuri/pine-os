#include "services/DeviceLockService.hpp"
#include "platform/WifiScanResults.hpp"
#include "platform/desktop/DesktopNetwork.hpp"
#include "ui/Shell.hpp"
#include <SDL3/SDL.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

#define CHECK(x) do{if(!(x))throw std::runtime_error("Check failed at line "+std::to_string(__LINE__)+": " #x);}while(false)
using namespace Pine;
static void complete(DeviceLockService& lock,std::uint64_t now){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    while(lock.busy()&&std::chrono::steady_clock::now()<deadline){SDL_Delay(2);lock.update(now);}
    CHECK(!lock.busy());
}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("pine-connectivity-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try{
        std::vector<WifiNetwork> networks{{"Home","WPA2",-80,true},{"Home","WPA2",-25,true},{"Home","Open",-50,true},{"","Open",-20,true},{"Cafe","Open",-60,true}};
        normalizeWifiResults(networks);CHECK(networks.size()==3);CHECK(networks[0].rssi==-25);
        CHECK(networks[1].security=="Open");
        for(int i=0;i<100;++i)networks.push_back({"AP"+std::to_string(i),"WPA2",-90,true});
        normalizeWifiResults(networks);CHECK(networks.size()==32);
        DesktopNetwork network;network.startScan();CHECK(network.scanSnapshot().networks.size()==3);
        CHECK(network.configure("Guest Wi-Fi",""));CHECK(network.networkName()=="Guest Wi-Fi");
        CHECK(!network.configure("Home","short"));network.setWifiEnabled(false);CHECK(network.scanSnapshot().networks.empty());
        network.startScan();CHECK(!network.scanSnapshot().scanning);
        CHECK(!DeviceLockService::validPin("12345"));CHECK(!DeviceLockService::validPin("abcdef"));CHECK(DeviceLockService::validPin("000000"));
        {
            DeviceLockService lock(root);complete(lock,1000);CHECK(lock.ready()&&lock.locked());
            CHECK(lock.unlock("123456",1000));complete(lock,1000);CHECK(!lock.locked());
            CHECK(lock.changePin("000000","654321",1001));complete(lock,1001);CHECK(lock.revision()==0);
            CHECK(lock.changePin("123456","654321",1002));complete(lock,1002);CHECK(lock.revision()==1);
            lock.lock();CHECK(lock.unlock("123456",1003));complete(lock,1003);CHECK(lock.locked());
            CHECK(lock.unlock("654321",1004));lock.lock();complete(lock,1004);CHECK(lock.locked()); // Stale unlock cannot defeat a newer lock.
            CHECK(lock.unlock("654321",1005));complete(lock,1005);CHECK(!lock.locked());
        }
        {
            DeviceLockService lock(root);CHECK(lock.ready()&&lock.locked());
            for(int i=0;i<5;++i){CHECK(lock.unlock("000000",2000));complete(lock,2000);}
            CHECK(lock.cooldownSeconds(2000)==30);CHECK(!lock.unlock("654321",2001));
            CHECK(lock.unlock("654321",32000));complete(lock,32000);CHECK(!lock.locked());
        }
        std::filesystem::create_directories(root/"corrupt"/"device-lock");
        std::ofstream(root/"corrupt"/"device-lock"/"security.json")<<"broken";
        {DeviceLockService lock(root/"corrupt");CHECK(!lock.ready()&&lock.locked());CHECK(!lock.unlock("123456",0));}
        CHECK(SDL_Init(SDL_INIT_VIDEO));
        auto* surface=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565);
        auto* renderer=SDL_CreateSoftwareRenderer(surface);CHECK(renderer);
        {
            Configuration config(root/"ui");config.load();Shell shell(nullptr,renderer,config,createPlatform("desktop"));
            shell.acceptanceSkipBoot();complete(shell.deviceLock(),SDL_GetTicks());
            CHECK(!shell.launchApp("settings"));
            SDL_Event event{};event.type=SDL_EVENT_KEY_DOWN;event.key.key=SDLK_F12;shell.handleEvent(event);
            bool called=false;shell.openTextPrompt("BYPASS","",[&](const std::string&){called=true;});CHECK(!shell.textInput().focused());
            auto tap=[&](float x,float y){SDL_Event click{};click.type=SDL_EVENT_MOUSE_BUTTON_UP;click.button.button=SDL_BUTTON_LEFT;click.button.x=x;click.button.y=y;shell.handleEvent(click);shell.render();};
            tap(360,1230);CHECK(shell.deviceLock().locked()); // Home cannot dismiss lock.
            for(int digit=1;digit<=6;++digit)tap(160+((digit-1)%3)*200,550+((digit-1)/3)*120);
            tap(550,910);complete(shell.deviceLock(),SDL_GetTicks());CHECK(!shell.deviceLock().locked());CHECK(!called);
            CHECK(shell.launchApp("settings"));
            auto enterPin=[&](const std::string& pin){for(char c:pin){const int digit=c-'0';if(digit==0)tap(360,910);else tap(160+((digit-1)%3)*200,550+((digit-1)/3)*120);}tap(550,910);};
            tap(395,250); // Change PIN, using the same keypad for all three stages.
            enterPin("123456");enterPin("654321");enterPin("654321");
            complete(shell.deviceLock(),SDL_GetTicks());shell.update(.001);
            CHECK(shell.deviceLock().revision()==1);CHECK(shell.launchApp("settings"));
            tap(370,320);CHECK(shell.network().scanSnapshot().networks.size()==3);
            shell.lockDevice();CHECK(shell.deviceLock().locked());CHECK(shell.apps().current()==nullptr);
            event.key.key=SDLK_ESCAPE;shell.handleEvent(event);CHECK(shell.deviceLock().locked());
        }
        SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);SDL_Quit();
        std::filesystem::remove_all(root);
        std::cout<<"Wi-Fi discovery and device PIN regression checks passed.\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
