#include "apps/Apps.hpp"
#include "core/UiContext.hpp"
#include "ui/Shell.hpp"
namespace Pine {void BluetoothApp::render(UiContext&ui){ui.renderBluetooth();}
void Shell::renderBluetooth(){
  label(45,105,"BLUETOOTH",5,Theme::Gold);if(button({485,115,180,55},bluetooth_.enabled()?"ON":"OFF",bluetooth_.enabled())){bluetooth_.setEnabled(!bluetooth_.enabled());persist();}
  sublabel(48,180,bluetooth_.enabled()?"SIMULATED DESKTOP ADAPTER":"BLUETOOTH DISABLED");if(bluetooth_.enabled()&&button({45,220,220,60},"SCAN",true))bluetooth_.startScan();
  float y=320;label(45,y,"DEVICES",3.5f);y+=55;if(bluetooth_.devices().empty())sublabel(48,y,"RUN A SCAN TO FIND DEVICES");
  for(const auto&d:bluetooth_.devices()){panel({40,y,640,95});label(65,y+20,d.name,3);sublabel(65,y+55,d.type+(d.connected?" - CONNECTED":" - AVAILABLE"));if(button({500,y+22,150,52},d.connected?"DISCONNECT":"CONNECT",d.connected)){if(d.connected)bluetooth_.disconnect(d.id);else bluetooth_.connect(d.id);}y+=110;}
}}
