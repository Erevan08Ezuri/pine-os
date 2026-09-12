#pragma once
#include "platform/Platform.hpp"
namespace Pine {
class DesktopBluetooth final:public BluetoothBackend {
public:
    bool enabled()const override{return enabled_;}
    BluetoothSnapshot snapshot()const override{return {enabled_,false,false,"SIMULATED DESKTOP ADAPTER",status_,devices_};}
    void setEnabled(bool)override;
    void startScan()override;
    void stopScan()override{}
    bool connect(const std::string&)override;
    bool disconnect(const std::string&)override;
private:
    bool enabled_{true};
    std::string status_{"Ready - tap Scan"};
    std::vector<BluetoothDevice> devices_;
};
}
