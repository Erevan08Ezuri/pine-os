#include "core/Configuration.hpp"
#include "core/Logger.hpp"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Pine {
Configuration::Configuration(std::filesystem::path root):dataRoot_(std::move(root)),storageRoot_(dataRoot_/"storage"),settingsPath_(dataRoot_/"settings.json"){}
void Configuration::validate(){settings_.brightness=std::clamp(settings_.brightness,0,100);settings_.volume=std::clamp(settings_.volume,0,100);if(settings_.deviceName.empty()||settings_.deviceName.size()>64)settings_.deviceName="Pine";}
void Configuration::load(){
  std::filesystem::create_directories(dataRoot_); usedFallback_=false;
  try { std::ifstream in(settingsPath_); if(!in) { usedFallback_=true; save(); return; } nlohmann::json j; in>>j;
    settings_.deviceName=j.value("deviceName",settings_.deviceName); settings_.brightness=j.value("brightness",settings_.brightness);
    settings_.volume=j.value("volume",settings_.volume); settings_.muted=j.value("muted",settings_.muted);
    settings_.bluetoothEnabled=j.value("bluetoothEnabled",settings_.bluetoothEnabled); settings_.wifiEnabled=j.value("wifiEnabled",settings_.wifiEnabled);
    settings_.developerMode=j.value("developerMode",settings_.developerMode); validate();
    Logger::instance().info("CONFIG","User configuration loaded");
  } catch(const std::exception& e){ usedFallback_=true; settings_=Settings{}; Logger::instance().warn("CONFIG",std::string("Malformed configuration; safe defaults loaded: ")+e.what()); save(); }
}
void Configuration::save()const{
  std::filesystem::create_directories(dataRoot_); const nlohmann::json j={{"deviceName",settings_.deviceName},{"brightness",settings_.brightness},{"volume",settings_.volume},{"muted",settings_.muted},{"bluetoothEnabled",settings_.bluetoothEnabled},{"wifiEnabled",settings_.wifiEnabled},{"developerMode",settings_.developerMode}};
  const auto temp=settingsPath_.string()+".tmp"; {std::ofstream out(temp,std::ios::trunc); if(!out)throw std::runtime_error("Cannot write settings");out<<j.dump(2)<<'\n';}
  std::error_code ec;std::filesystem::remove(settingsPath_,ec);std::filesystem::rename(temp,settingsPath_);
}
}
