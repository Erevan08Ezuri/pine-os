#include "finance/Money.hpp"
#include <algorithm>
#include <cctype>
#include <format>
#include <limits>
#include <stdexcept>

namespace Pine::Finance {
Currency Currency::fromCode(std::string code){std::ranges::transform(code,code.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});if(code=="JPY"||code=="KRW")return{code,0};if(code=="BHD"||code=="KWD"||code=="JOD")return{code,3};return{code.empty()?"USD":code,2};}
Money Money::operator+(const Money&o)const{if(currency.code!=o.currency.code)throw std::invalid_argument("Currency mismatch");if((o.minor>0&&minor>std::numeric_limits<std::int64_t>::max()-o.minor)||(o.minor<0&&minor<std::numeric_limits<std::int64_t>::min()-o.minor))throw std::overflow_error("Money overflow");return{minor+o.minor,currency};}
Money Money::operator-(const Money& o)const {
    if(currency.code!=o.currency.code)throw std::invalid_argument("Currency mismatch");
    if((o.minor>0&&minor<std::numeric_limits<std::int64_t>::min()+o.minor)||
       (o.minor<0&&minor>std::numeric_limits<std::int64_t>::max()+o.minor))throw std::overflow_error("Money overflow");
    return {minor-o.minor,currency};
}
std::optional<std::int64_t> parseMoneyMinor(const std::string&input,const Currency&currency){if(currency.minorDigits<0||currency.minorDigits>3)return{};std::string s;for(unsigned char c:input)if(!std::isspace(c)&&c!='$'&&c!=',')s.push_back(static_cast<char>(c));if(s.empty())return{};bool neg=false;if(s.front()=='+'||s.front()=='-'){neg=s.front()=='-';s.erase(s.begin());}if(s.empty())return{};auto dot=s.find('.');if(dot!=std::string::npos&&s.find('.',dot+1)!=std::string::npos)return{};std::string whole=dot==std::string::npos?s:s.substr(0,dot),fraction=dot==std::string::npos?"":s.substr(dot+1);if(whole.empty()&&fraction.empty())return{};if(whole.empty())whole="0";if(!std::ranges::all_of(whole,[](unsigned char c){return std::isdigit(c);})||!std::ranges::all_of(fraction,[](unsigned char c){return std::isdigit(c);})||fraction.size()>static_cast<std::size_t>(currency.minorDigits))return{};while(fraction.size()<static_cast<std::size_t>(currency.minorDigits))fraction.push_back('0');try{std::int64_t scale=1;for(int i=0;i<currency.minorDigits;++i)scale*=10;const auto major=std::stoll(whole);const auto part=fraction.empty()?0:std::stoll(fraction);if(major>(std::numeric_limits<std::int64_t>::max()-part)/scale)return{};auto result=major*scale+part;return neg?-result:result;}catch(...){return{};}}
std::string formatMoney(std::int64_t minor,const Currency&currency,bool signedValue){const bool neg=minor<0;const auto absolute=neg?static_cast<std::uint64_t>(-(minor+1))+1:static_cast<std::uint64_t>(minor);std::uint64_t scale=1;for(int i=0;i<currency.minorDigits;++i)scale*=10;const auto whole=absolute/scale,part=absolute%scale;const std::string sign=neg?"-":(signedValue&&minor>0?"+":"");if(currency.minorDigits==0)return std::format("{}{} {}",sign,whole,currency.code);return std::format("{}{}.{:0{}} {}",sign,whole,part,currency.minorDigits,currency.code);}
}
