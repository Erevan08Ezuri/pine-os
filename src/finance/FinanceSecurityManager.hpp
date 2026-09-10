#pragma once
#include "finance/FinanceModels.hpp"
#include "services/SecurityService.hpp"
#include <cstdint>
namespace Pine::Finance {class FinanceSecurityManager{public:explicit FinanceSecurityManager(SecurityService&service):service_(service){}void configure(const FinanceSettings&s){settings_=s;}bool unlock(const std::string&pin,std::uint64_t now);void lock(){locked_=true;}void onOpen(std::uint64_t now);void onBackground(std::uint64_t now);void update(std::uint64_t now);bool locked()const{return settings_.requireAuthentication&&locked_;}bool authenticationAvailable()const{return service_.hasDevicePin();}private:std::uint64_t timeout()const;SecurityService&service_;FinanceSettings settings_;bool locked_{true};std::uint64_t backgroundAt_{};};}
