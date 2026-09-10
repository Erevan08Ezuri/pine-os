#pragma once
#include "platform/Platform.hpp"
namespace Pine {class BluetoothService{public:explicit BluetoothService(BluetoothBackend&b):backend_(b){}bool enabled()const{return backend_.enabled();}void setEnabled(bool v){backend_.setEnabled(v);}void startScan(){backend_.startScan();}const std::vector<BluetoothDevice>&devices()const{return backend_.devices();}bool connect(const std::string&id){return backend_.connect(id);}bool disconnect(const std::string&id){return backend_.disconnect(id);}private:BluetoothBackend&backend_;};}
