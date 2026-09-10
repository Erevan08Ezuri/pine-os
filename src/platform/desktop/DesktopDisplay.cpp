#include "platform/desktop/DesktopDisplay.hpp"
#include <algorithm>
namespace Pine {void DesktopDisplay::setBrightness(int value){brightness_=std::clamp(value,0,100);}}
