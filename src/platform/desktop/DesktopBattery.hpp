#pragma once
#include "platform/Platform.hpp"
namespace Pine { class DesktopBattery final:public BatteryBackend{public:int percentage()const override{return percentage_;}bool charging()const override{return charging_;}void setPercentage(int)override;void setCharging(bool v)override{charging_=v;}private:int percentage_{80};bool charging_{false};}; }
