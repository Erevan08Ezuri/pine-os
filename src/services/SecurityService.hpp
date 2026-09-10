#pragma once
#include <filesystem>
#include <string>
namespace Pine {class SecurityService{public:explicit SecurityService(std::filesystem::path dataRoot);bool hasDevicePin()const;bool setDevicePin(const std::string&pin);bool authenticate(const std::string&pin)const;private:std::filesystem::path path_;std::string salt_,hash_;void load();};}
