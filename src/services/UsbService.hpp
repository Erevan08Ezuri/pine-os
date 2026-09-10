#pragma once
#include "platform/Platform.hpp"
namespace Pine {class UsbService{public:explicit UsbService(UsbBackend&b):backend_(b){}UsbMode mode()const{return backend_.mode();}void setMode(UsbMode m){backend_.setMode(m);}void cycle();std::string modeName()const;private:UsbBackend&backend_;};}
