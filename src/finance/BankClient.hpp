#pragma once
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

namespace Pine::Finance {
// The device knows only its private bridge credential, never Plaid credentials or bank passwords.
class BankClient {
public:
  explicit BankClient(std::filesystem::path dataRoot):path_(std::move(dataRoot)/"bank-connection.json"){}
  bool configured()const;
  void configure(const std::string&url,const std::string&token)const;
  nlohmann::json request(const std::string&method,const std::string&path)const;
private:
  std::filesystem::path path_;
};
}
