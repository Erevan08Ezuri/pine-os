#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopBluetooth final:public BluetoothBackend{public:bool enabled()const override{return enabled_;}void setEnabled(bool)override;void startScan()override;const std::vector<BluetoothDevice>& devices()const override{return devices_;}bool connect(const std::string&)override;bool disconnect(const std::string&)override;private:bool enabled_{true};std::vector<BluetoothDevice> devices_;};}
