#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include <algorithm>
namespace Pine {void SettingsApp::render(UiContext&ui){ui.renderSettings();}
void Shell::renderSettings(){
  label(45,105,"SETTINGS",6,Theme::Gold);sublabel(48,165,"SYSTEM CONTROLS AND INFORMATION");float y=215;
  auto valueRow=[&](const std::string&name,const std::string&value){panel({40,y,640,68});label(65,y+22,name,2.5f);label(650-textWidth(value,2.2f),y+24,value,2.2f,Theme::Gold);y+=78;};
  panel({40,y,640,68});label(65,y+22,"WIFI",2.5f);if(button({475,y+10,175,48},network_.wifiEnabled()?"ON":"OFF",network_.wifiEnabled())){network_.setWifiEnabled(!network_.wifiEnabled());persist();}y+=78;
  panel({40,y,640,68});label(65,y+22,"BLUETOOTH",2.5f);if(button({475,y+10,175,48},bluetooth_.enabled()?"ON":"OFF",bluetooth_.enabled())){bluetooth_.setEnabled(!bluetooth_.enabled());persist();}y+=78;
  panel({40,y,640,82});label(65,y+18,"DISPLAY BRIGHTNESS",2.3f);meter({300,y+28,205,12},display_.brightness());if(button({525,y+15,55,48},"-")){display_.setBrightness(display_.brightness()-5);persist();}if(button({590,y+15,55,48},"+",true)){display_.setBrightness(display_.brightness()+5);persist();}y+=92;
  panel({40,y,640,82});label(65,y+18,"SOUND VOLUME",2.3f);meter({300,y+28,205,12},audio_.volume());if(button({525,y+15,55,48},"-")){audio_.setVolume(audio_.volume()-5);persist();}if(button({590,y+15,55,48},"+",true)){audio_.setVolume(audio_.volume()+5);persist();}y+=92;
  panel({40,y,640,68});label(65,y+22,"MUTE",2.5f);if(button({475,y+10,175,48},audio_.muted()?"MUTED":"ACTIVE")){audio_.setMuted(!audio_.muted());persist();}y+=78;
  valueRow("BATTERY",std::to_string(battery_.percentage())+"% "+(battery_.isCharging()?"CHARGING":"BATTERY"));
  valueRow("STORAGE",std::to_string(files_.usedBytes()/1024)+" KB USED");
  panel({40,y,640,68});label(65,y+22,"USB",2.5f);if(button({390,y+10,260,48},usb_.modeName()))usb_.cycle();y+=78;
  valueRow("DEVICE",system_.deviceName()+" / "+system_.version());valueRow("PLATFORM",system_.deviceInfo().platform+" / "+system_.deviceInfo().architecture);
}}
