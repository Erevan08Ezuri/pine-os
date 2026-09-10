#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DisplayService{public:explicit DisplayService(DisplayBackend&b):backend_(b){}int brightness()const{return backend_.brightness();}void setBrightness(int value){backend_.setBrightness(value);}private:DisplayBackend&backend_;};}
