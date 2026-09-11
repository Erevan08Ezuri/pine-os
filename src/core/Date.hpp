#pragma once
#include <chrono>
#include <stdexcept>
#include <string_view>
namespace Pine {
inline std::chrono::sys_days parseIsoDate(std::string_view s){
    if(s.size()!=10||s[4]!='-'||s[7]!='-')throw std::invalid_argument("Expected YYYY-MM-DD");
    auto number=[&](int start,int count){
        int n=0;for(int i=start;i<start+count;++i){if(s[i]<'0'||s[i]>'9')throw std::invalid_argument("Invalid date");n=n*10+s[i]-'0';}return n;
    };
    const auto date=std::chrono::year(number(0,4))/number(5,2)/number(8,2);
    if(!date.ok())throw std::invalid_argument("Invalid calendar date");
    return std::chrono::sys_days(date);
}
}
