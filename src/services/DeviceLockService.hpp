#pragma once
#include "services/SecurityService.hpp"
#include <cstdint>
#include <future>
#include <string>

namespace Pine {
class DeviceLockService {
public:
    explicit DeviceLockService(const std::filesystem::path& dataRoot);
    ~DeviceLockService();
    bool locked()const{return locked_;}
    bool busy()const{return job_.valid();}
    bool ready()const{return ready_;}
    const std::string& status()const{return status_;}
    std::uint64_t revision()const{return revision_;}
    unsigned cooldownSeconds(std::uint64_t now)const;
    void update(std::uint64_t now);
    void lock();
    bool unlock(std::string pin,std::uint64_t now);
    bool changePin(std::string current,std::string replacement,std::uint64_t now);
    static bool validPin(const std::string& pin);
private:
    enum class Operation{Initialize,Unlock,Change};
    enum class Result{Success,WrongPin,StorageError};
    SecurityService security_;
    std::future<Result> job_;
    Operation operation_{Operation::Initialize};
    bool ready_{},locked_{true};
    unsigned failures_{};
    std::uint64_t blockedUntil_{},generation_{},jobGeneration_{},revision_{};
    std::string status_{"Preparing device PIN..."};
};
}
