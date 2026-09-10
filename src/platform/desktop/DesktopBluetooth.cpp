#include "platform/desktop/DesktopBluetooth.hpp"
#include "core/Logger.hpp"
#include <algorithm>
namespace Pine {
void DesktopBluetooth::setEnabled(bool value){enabled_=value;if(!enabled_)for(auto&d:devices_)d.connected=false;Logger::instance().info("BLUETOOTH",enabled_?"Bluetooth enabled":"Bluetooth disabled; connections closed");}
void DesktopBluetooth::startScan(){if(!enabled_)return;devices_={{"pine-buds","Pine Buds","Audio",false},{"car-audio","Car Audio","Audio",false},{"bt-keyboard","BT Keyboard","Input",false},{"workshop-speaker","Workshop Speaker","Audio",false}};Logger::instance().info("BLUETOOTH","Simulation scan found 4 devices");}
bool DesktopBluetooth::connect(const std::string&id){if(!enabled_)return false;auto it=std::ranges::find_if(devices_,[&](auto&d){return d.id==id;});if(it==devices_.end())return false;it->connected=true;return true;}
bool DesktopBluetooth::disconnect(const std::string&id){auto it=std::ranges::find_if(devices_,[&](auto&d){return d.id==id;});if(it==devices_.end())return false;it->connected=false;return true;}
}
