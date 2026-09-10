#pragma once
#include "platform/Platform.hpp"
#include "platform/desktop/DesktopAudio.hpp"
#include "platform/desktop/DesktopBattery.hpp"
#include "platform/desktop/DesktopBluetooth.hpp"
#include "platform/desktop/DesktopCamera.hpp"
#include "platform/desktop/DesktopNetwork.hpp"
#include "platform/desktop/DesktopUsb.hpp"
#include "platform/desktop/DesktopDisplay.hpp"
namespace Pine {class DesktopPlatform final:public Platform{public:DesktopPlatform();std::string name()const override{return "Desktop Simulator";}DeviceInformation deviceInfo()const override;BatteryBackend&battery()override{return battery_;}CameraBackend&camera()override{return camera_;}BluetoothBackend&bluetooth()override{return bluetooth_;}AudioBackend&audio()override{return audio_;}NetworkBackend&network()override{return network_;}UsbBackend&usb()override{return usb_;}DisplayBackend&display()override{return display_;}private:DesktopBattery battery_;DesktopCamera camera_;DesktopBluetooth bluetooth_;DesktopAudio audio_;DesktopNetwork network_;DesktopUsb usb_;DesktopDisplay display_;};}
