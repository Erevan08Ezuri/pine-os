#pragma once
#include "platform/BleCentralState.hpp"
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

struct ble_gap_event;
namespace Pine {
class Tab5Bluetooth final:public BluetoothBackend {
public:
    Tab5Bluetooth();
    ~Tab5Bluetooth()override;
    BluetoothSnapshot snapshot()const override;
    bool enabled()const override;
    void setEnabled(bool)override;
    void startScan()override;
    void stopScan()override;
    bool connect(const std::string&)override;
    bool disconnect(const std::string&)override;
private:
    enum class Command {None,Scan,Connect,Disconnect};
    mutable std::mutex mutex_;
    std::condition_variable wake_;
    BleCentralState state_;
    std::thread worker_,host_;
    bool stopping_{},disablePending_{},cancelPending_{},synced_{},initialized_{};
    Command command_{Command::None};
    std::string target_,connectingId_,connectedId_;
    std::uint64_t generation_{},connectGeneration_{};
    std::uint16_t handle_{0xffff};
    std::uint8_t ownAddressType_{};
    static Tab5Bluetooth* instance_;
    static void onSync();
    static void onReset(int reason);
    static int onGap(ble_gap_event*,void*);
    void run();
    bool initialize(std::uint64_t generation);
    void fail(const std::string&,std::uint64_t generation);
    void cancelOperations();
};
}
