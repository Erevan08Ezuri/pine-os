#include "ui/Shell.hpp"
#include <format>
namespace Pine {void renderDeveloperPanel(Shell&s){s.label(45,105,"DEVELOPER PANEL",5,Theme::Gold);s.sublabel(48,160,"F12 TO CLOSE - LIVE SERVICE STATE");s.panel({40,215,640,770});float y=250;
auto row=[&](const std::string&name,const std::string&value){s.label(70,y,name,2.7f);s.label(390,y,value,2.7f,Theme::Gold);y+=75;};
row("PLATFORM",s.system().deviceInfo().platform);row("FPS",std::format("{:.0f}",s.fps()));row("CURRENT APP",s.apps().currentId());row("VERSION",s.system().version());row("STORAGE ROOT","DATA/STORAGE");
s.label(70,y,"BATTERY",2.7f);s.label(350,y,std::to_string(s.battery().percentage())+"%",2.7f);if(s.button({470,y-15,70,48},"-"))s.battery().simulatePercentage(s.battery().percentage()-5);if(s.button({555,y-15,70,48},"+",true))s.battery().simulatePercentage(s.battery().percentage()+5);y+=75;
if(s.button({70,y-15,555,50},s.battery().isCharging()?"CHARGING ON":"CHARGING OFF"))s.battery().simulateCharging(!s.battery().isCharging());y+=65;
if(s.button({70,y-15,265,50},s.network().wifiEnabled()?"WIFI ON":"WIFI OFF"))s.network().setWifiEnabled(!s.network().wifiEnabled());if(s.button({360,y-15,265,50},s.bluetooth().enabled()?"BT ON":"BT OFF"))s.bluetooth().setEnabled(!s.bluetooth().enabled());y+=65;
if(s.button({70,y-15,555,50},"USB "+s.usb().modeName()))s.usb().cycle();y+=65;if(s.button({70,y-15,555,50},s.camera().available()?"CAMERA AVAILABLE":"CAMERA UNAVAILABLE"))s.camera().simulateAvailable(!s.camera().available());y+=65;if(s.button({70,y-15,555,50},s.textInput().forceSoftwareKeyboard()?"SOFT KEYBOARD FORCED":"SOFT KEYBOARD AUTO"))s.textInput().setForceSoftwareKeyboard(!s.textInput().forceSoftwareKeyboard());}
}
