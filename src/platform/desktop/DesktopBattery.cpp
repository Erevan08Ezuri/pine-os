#include "platform/desktop/DesktopBattery.hpp"
#include <algorithm>
namespace Pine {void DesktopBattery::setPercentage(int v){percentage_=std::clamp(v,0,100);} }
