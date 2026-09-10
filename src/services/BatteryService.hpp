#pragma once
#include "platform/Platform.hpp"
namespace Pine {class BatteryService{public:explicit BatteryService(BatteryBackend&b):backend_(b){}int percentage()const{return backend_.percentage();}bool isCharging()const{return backend_.charging();}void simulatePercentage(int v){backend_.setPercentage(v);}void simulateCharging(bool v){backend_.setCharging(v);}private:BatteryBackend&backend_;};}
