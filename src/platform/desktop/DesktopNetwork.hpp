#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopNetwork final:public NetworkBackend{public:bool wifiEnabled()const override{return enabled_;}void setWifiEnabled(bool v)override{enabled_=v;}bool connected()const override{return enabled_;}std::string networkName()const override{return enabled_?"Desktop Host Network":"Not connected";}private:bool enabled_{true};};}
