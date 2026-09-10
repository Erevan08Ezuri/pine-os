#pragma once
#include <cstdint>
#include <optional>
#include <string>

namespace Pine::Finance {
struct Currency { std::string code{"USD"}; int minorDigits{2}; static Currency fromCode(std::string code); };
struct Money {
  std::int64_t minor{}; Currency currency{};
  Money operator+(const Money& other) const;
  Money operator-(const Money& other) const;
};
std::optional<std::int64_t> parseMoneyMinor(const std::string& value,const Currency& currency);
std::string formatMoney(std::int64_t minor,const Currency& currency,bool signedValue=false);
}
