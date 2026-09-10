#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopDisplay final:public DisplayBackend{public:int brightness()const override{return brightness_;}void setBrightness(int)override;private:int brightness_{65};};}
