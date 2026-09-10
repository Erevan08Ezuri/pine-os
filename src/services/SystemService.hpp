#pragma once
#include "core/Configuration.hpp"
#include "platform/Platform.hpp"
#include <string>
namespace Pine {class SystemService{public:SystemService(Configuration&c,Platform&p):config_(c),platform_(p){}const std::string&deviceName()const{return config_.settings().deviceName;}std::string version()const;DeviceInformation deviceInfo()const{return platform_.deviceInfo();}bool developerMode()const{return config_.settings().developerMode;}private:Configuration&config_;Platform&platform_;};}
