#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include <algorithm>
namespace Pine {void SettingsApp::render(UiContext&ui){ui.renderSettings();}
void Shell::renderSettings(){
  label(45,105,"SETTINGS",6,Theme::Gold);sublabel(48,165,"SYSTEM CONTROLS AND INFORMATION");float y=215;
  auto valueRow=[&](const std::string&name,const std::string&value){panel({40,y,640,68});label(65,y+22,name,2.5f);label(650-textWidth(value,2.2f),y+24,value,2.2f,Theme::Gold);y+=78;};
#ifdef PINE_TAB5
  panel({40,y,640,110});label(65,y+18,"WIFI",2.5f);sublabel(65,y+72,network_.networkName().substr(0,42));
  if(button({290,y+10,170,48},"CONNECT",true)){
    openTextPrompt("WI-FI NETWORK NAME","",[this](const std::string& ssid){
      openTextPrompt("WI-FI PASSWORD","",[this,ssid](const std::string& password){
        if(network_.configure(ssid,password)){persist();toast("CONNECTING TO WI-FI");}
        else toast("INVALID NETWORK DETAILS");
      },InputType::Password,63);
    },InputType::Text,32);
  }
  if(button({475,y+10,175,48},network_.wifiEnabled()?"ON":"OFF")){network_.setWifiEnabled(!network_.wifiEnabled());persist();}
  y+=120;
#else
  panel({40,y,640,68});label(65,y+22,"WIFI",2.5f);if(button({475,y+10,175,48},network_.wifiEnabled()?"ON":"OFF",network_.wifiEnabled())){network_.setWifiEnabled(!network_.wifiEnabled());persist();}y+=78;
#endif
#ifndef PINE_TAB5
  panel({40,y,640,68});label(65,y+22,"BLUETOOTH",2.5f);if(button({475,y+10,175,48},bluetooth_.enabled()?"ON":"OFF",bluetooth_.enabled())){bluetooth_.setEnabled(!bluetooth_.enabled());persist();}y+=78;
#else
  valueRow("BLUETOOTH","NOT SUPPORTED");
#endif
  panel({40,y,640,82});label(65,y+18,"DISPLAY BRIGHTNESS",2.3f);const auto brightness=display_.brightness();meter({300,y+28,160,12},brightness);label(472,y+20,std::to_string(brightness)+"%",2.0f,Theme::Gold);if(button({525,y+15,55,48},"-")){display_.setBrightness(std::max(10,brightness-10));persist();}if(button({590,y+15,55,48},"+",true)){display_.setBrightness(std::min(100,brightness+10));persist();}y+=92;
  panel({40,y,640,82});label(65,y+18,"SOUND VOLUME",2.3f);meter({300,y+28,205,12},audio_.volume());if(button({525,y+15,55,48},"-")){audio_.setVolume(audio_.volume()-5);persist();}if(button({590,y+15,55,48},"+",true)){audio_.setVolume(audio_.volume()+5);persist();}y+=92;
  panel({40,y,640,68});label(65,y+22,"MUTE",2.5f);if(button({475,y+10,175,48},audio_.muted()?"MUTED":"ACTIVE")){audio_.setMuted(!audio_.muted());persist();}y+=78;
  valueRow("BATTERY",battery_.percentage()<0?"LEVEL UNAVAILABLE":std::to_string(battery_.percentage())+"% "+(battery_.isCharging()?"CHARGING":"BATTERY"));
  valueRow("STORAGE",std::to_string(files_.usedBytes()/1024)+" KB USED");
#ifndef PINE_TAB5
  panel({40,y,640,68});label(65,y+22,"USB",2.5f);if(button({390,y+10,260,48},usb_.modeName()))usb_.cycle();y+=78;
#else
  valueRow("USB","FLASH / SERIAL");
#endif
  valueRow("DEVICE",system_.deviceName()+" / "+system_.version());valueRow("PLATFORM",system_.deviceInfo().platform+" / "+system_.deviceInfo().architecture);
}}
