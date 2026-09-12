#include "services/DeviceLockService.hpp"
#include <algorithm>
#include <chrono>

namespace Pine {
bool DeviceLockService::validPin(const std::string& pin){
    return pin.size()==6&&std::ranges::all_of(pin,[](char c){return c>='0'&&c<='9';});
}
DeviceLockService::DeviceLockService(const std::filesystem::path& root):security_(root/"device-lock"){
    const auto path=root/"device-lock"/"security.json";
    // Existing or damaged credentials must never silently reset to the default.
    const bool exists=std::filesystem::exists(path)||std::filesystem::exists(path.string()+".bak");
    if(security_.hasDevicePin()){ready_=true;status_="Enter your PIN";return;}
    if(exists){status_="PIN storage needs recovery";return;}
    try{
        job_=std::async(std::launch::async,[this]{
            try{return security_.setDevicePin("123456")?Result::Success:Result::StorageError;}
            catch(...){return Result::StorageError;}
        });
    }catch(...){status_="Could not initialize PIN worker";}
}
DeviceLockService::~DeviceLockService(){if(job_.valid())job_.wait();}
unsigned DeviceLockService::cooldownSeconds(std::uint64_t now)const{
    return now<blockedUntil_?static_cast<unsigned>((blockedUntil_-now+999)/1000):0;
}
void DeviceLockService::lock(){locked_=true;++generation_;if(!busy()&&ready_)status_="Enter your PIN";}
bool DeviceLockService::unlock(std::string pin,std::uint64_t now){
    if(!ready_||busy()||!locked_||cooldownSeconds(now))return false;
    if(!validPin(pin)){status_="Enter six digits";return false;}
    operation_=Operation::Unlock;jobGeneration_=generation_;status_="Checking PIN...";
    try{
        job_=std::async(std::launch::async,[this,pin=std::move(pin)]{
            try{return security_.authenticate(pin)?Result::Success:Result::WrongPin;}
            catch(...){return Result::StorageError;}
        });
    }catch(...){status_="Could not start PIN check";return false;}
    return true;
}
bool DeviceLockService::changePin(std::string current,std::string replacement,std::uint64_t now){
    if(!ready_||busy()||locked_||cooldownSeconds(now))return false;
    if(!validPin(current)||!validPin(replacement)){status_="Enter six digits";return false;}
    operation_=Operation::Change;jobGeneration_=generation_;status_="Saving PIN...";
    try{
        job_=std::async(std::launch::async,[this,current=std::move(current),replacement=std::move(replacement)]{
            try{
                if(!security_.authenticate(current))return Result::WrongPin;
                return security_.setDevicePin(replacement)?Result::Success:Result::StorageError;
            }catch(...){return Result::StorageError;}
        });
    }catch(...){status_="Could not start PIN change";return false;}
    return true;
}
void DeviceLockService::update(std::uint64_t now){
    if(!busy()||job_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
    const auto result=job_.get();
    if(operation_==Operation::Initialize){
        ready_=result==Result::Success;status_=ready_?"Enter your PIN":"Could not save initial PIN";return;
    }
    if(result==Result::Success){
        failures_=0;blockedUntil_=0;
        if(operation_==Operation::Unlock){
            if(jobGeneration_==generation_){locked_=false;status_="Unlocked";}
            else status_="Enter your PIN";
        }
        if(operation_==Operation::Change){++revision_;status_="PIN changed";}
    }else if(result==Result::WrongPin){
        ++failures_;if(failures_>=5)blockedUntil_=now+std::min(300000u,30000u*(failures_-4));
        status_="Incorrect PIN";
    }else status_="PIN storage error - try again";
}
}
