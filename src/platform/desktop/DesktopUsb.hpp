#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopUsb final:public UsbBackend{public:UsbMode mode()const override{return mode_;}void setMode(UsbMode v)override{mode_=v;}private:UsbMode mode_{UsbMode::Disconnected};};}
