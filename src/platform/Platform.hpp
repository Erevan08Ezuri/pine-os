#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
namespace Pine {
struct BluetoothDevice { std::string id,name,type; bool connected{false}; };
struct CameraFrame { int width{},height{}; std::vector<std::uint16_t> pixels; };
enum class UsbMode { Disconnected, ChargingOnly, FileTransfer };
class BatteryBackend { public:virtual~BatteryBackend()=default;virtual int percentage()const=0;virtual bool charging()const=0;virtual void setPercentage(int)=0;virtual void setCharging(bool)=0;};
class CameraBackend { public:virtual~CameraBackend()=default;virtual bool available()const=0;virtual bool start()=0;virtual void stop()=0;virtual bool capturePhoto(const std::filesystem::path&)=0;virtual void setAvailable(bool)=0;virtual bool previewFrame(CameraFrame&){return false;}};
class BluetoothBackend { public:virtual~BluetoothBackend()=default;virtual bool enabled()const=0;virtual void setEnabled(bool)=0;virtual void startScan()=0;virtual const std::vector<BluetoothDevice>& devices()const=0;virtual bool connect(const std::string&)=0;virtual bool disconnect(const std::string&)=0;};
class AudioBackend { public:virtual~AudioBackend()=default;virtual int volume()const=0;virtual void setVolume(int)=0;virtual bool muted()const=0;virtual void setMuted(bool)=0;};
class NetworkBackend { public:virtual~NetworkBackend()=default;virtual bool wifiEnabled()const=0;virtual void setWifiEnabled(bool)=0;virtual bool connected()const=0;virtual std::string networkName()const=0;virtual bool configure(const std::string&,const std::string&){return false;}};
class UsbBackend { public:virtual~UsbBackend()=default;virtual UsbMode mode()const=0;virtual void setMode(UsbMode)=0;};
class DisplayBackend { public:virtual~DisplayBackend()=default;virtual int brightness()const=0;virtual void setBrightness(int)=0;};
struct DeviceInformation {std::string platform,architecture;};
class Platform { public:virtual~Platform()=default;virtual std::string name()const=0;virtual DeviceInformation deviceInfo()const=0;virtual BatteryBackend& battery()=0;virtual CameraBackend& camera()=0;virtual BluetoothBackend& bluetooth()=0;virtual AudioBackend& audio()=0;virtual NetworkBackend& network()=0;virtual UsbBackend& usb()=0;virtual DisplayBackend& display()=0;};
std::unique_ptr<Platform> createPlatform(const std::string& target);
}
