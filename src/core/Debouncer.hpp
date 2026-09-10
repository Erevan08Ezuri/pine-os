#pragma once
#include <cstdint>
namespace Pine {class Debouncer{public:explicit Debouncer(std::uint64_t delayMs=750):delay_(delayMs){}void mark(std::uint64_t now){dirty_=true;last_=now;}bool ready(std::uint64_t now)const{return dirty_&&now>=last_&&now-last_>=delay_;}void clear(){dirty_=false;}bool dirty()const{return dirty_;}private:std::uint64_t delay_,last_{};bool dirty_{};};}
