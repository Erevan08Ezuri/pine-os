#pragma once
#include "platform/Platform.hpp"
namespace Pine {class NetworkService{public:explicit NetworkService(NetworkBackend&b):backend_(b){}bool wifiEnabled()const{return backend_.wifiEnabled();}void setWifiEnabled(bool v){backend_.setWifiEnabled(v);}bool connected()const{return backend_.connected();}std::string networkName()const{return backend_.networkName();}bool configure(const std::string& s,const std::string& p){return backend_.configure(s,p);}private:NetworkBackend&backend_;};}
