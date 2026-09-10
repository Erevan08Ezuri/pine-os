#pragma once
#include <filesystem>
#include <string>

namespace Pine {
struct Settings {
  std::string deviceName{"Pine"}; int brightness{65}; int volume{75}; bool muted{false};
  bool bluetoothEnabled{true}; bool wifiEnabled{true}; bool developerMode{true};
};
class Configuration {
public:
  explicit Configuration(std::filesystem::path dataRoot);
  void load(); void save() const; Settings& settings(){return settings_;} const Settings& settings()const{return settings_;}
  const std::filesystem::path& dataRoot()const{return dataRoot_;} const std::filesystem::path& storageRoot()const{return storageRoot_;}
  bool usedFallback()const{return usedFallback_;}
private:
  void validate(); std::filesystem::path dataRoot_,storageRoot_,settingsPath_; Settings settings_; bool usedFallback_{false};
};
}
