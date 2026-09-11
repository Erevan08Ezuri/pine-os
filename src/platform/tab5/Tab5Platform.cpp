#include "platform/tab5/Tab5Platform.hpp"
#include "platform/tab5/Tab5Network.hpp"
#include "bsp/m5stack_tab5.h"
#include <algorithm>
namespace Pine {
namespace {
// Unsupported capabilities report unavailable; desktop simulation is never linked.
class Battery final:public BatteryBackend{
public:int percentage()const override{return -1;}bool charging()const override{return false;}
    void setPercentage(int)override{}void setCharging(bool)override{}
};
class Camera final:public CameraBackend{
public:bool available()const override{return false;}bool start()override{return false;}
    void stop()override{}bool capturePhoto(const std::filesystem::path&)override{return false;}
    void setAvailable(bool)override{}
};
class Bluetooth final:public BluetoothBackend{
    std::vector<BluetoothDevice> devices_;
public:bool enabled()const override{return false;}void setEnabled(bool)override{}
    void startScan()override{}const std::vector<BluetoothDevice>& devices()const override{return devices_;}
    bool connect(const std::string&)override{return false;}bool disconnect(const std::string&)override{return false;}
};
class Audio final:public AudioBackend{
    esp_codec_dev_handle_t codec_{};int volume_{75};bool muted_{};
public:
    Audio(){codec_=bsp_audio_codec_speaker_init();}
    int volume()const override{return volume_;}
    void setVolume(int v)override{v=std::clamp(v,0,100);if(codec_&&esp_codec_dev_set_out_vol(codec_,v)==ESP_OK)volume_=v;}
    bool muted()const override{return muted_;}
    void setMuted(bool v)override{if(codec_&&esp_codec_dev_set_out_mute(codec_,v)==ESP_OK)muted_=v;}
};
class Usb final:public UsbBackend{
public:UsbMode mode()const override{return bsp_usb_c_detect()?UsbMode::ChargingOnly:UsbMode::Disconnected;}
    void setMode(UsbMode)override{}
};
class Display final:public DisplayBackend{
    int value_{100};
public:int brightness()const override{return value_;}
    void setBrightness(int v)override{v=std::clamp(v,5,100);if(bsp_display_brightness_set(v)==ESP_OK)value_=v;}
};
class Tab5 final:public Platform{
    Battery battery_;Camera camera_;Bluetooth bluetooth_;Audio audio_;Tab5Network network_;Usb usb_;Display display_;
public:
    std::string name()const override{return "tab5";}
    DeviceInformation deviceInfo()const override{return{"M5Stack Tab5","ESP32-P4 RISC-V"};}
    BatteryBackend& battery()override{return battery_;}CameraBackend& camera()override{return camera_;}
    BluetoothBackend& bluetooth()override{return bluetooth_;}AudioBackend& audio()override{return audio_;}
    NetworkBackend& network()override{return network_;}UsbBackend& usb()override{return usb_;}
    DisplayBackend& display()override{return display_;}
};
}
std::unique_ptr<Platform> createTab5Platform(){return std::make_unique<Tab5>();}
}
