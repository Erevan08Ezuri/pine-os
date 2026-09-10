#include "platform/desktop/DesktopAudio.hpp"
#include <algorithm>
namespace Pine {void DesktopAudio::setVolume(int v){volume_=std::clamp(v,0,100);} }
