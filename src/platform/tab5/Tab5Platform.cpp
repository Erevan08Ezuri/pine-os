#include "platform/tab5/Tab5Platform.hpp"
#include "platform/tab5/Tab5Network.hpp"
#include "platform/tab5/Tab5Camera.hpp"
#include "platform/tab5/Tab5Bluetooth.hpp"
#include "bsp/m5stack_tab5.h"
#include "esp_log.h"
#include <algorithm>
namespace Pine {
namespace {
// Unsupported capabilities report unavailable; desktop simulation is never linked.
class Battery final:public BatteryBackend{
public:int percentage()const override{return -1;}bool charging()const override{return false;}
    void setPercentage(int)override{}void setCharging(bool)override{}
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
public:
    int brightness()const override{return value_;}
    void setBrightness(int v)override{
        v=std::clamp(v,0,100);
        const auto result=bsp_display_brightness_set(v);
        if(result==ESP_OK)value_=v;
        else ESP_LOGW("PineDisplay","Unable to set brightness to %d%%: %s",v,esp_err_to_name(result));
    }
};
class Tab5 final:public Platform{
    Battery battery_;Tab5Camera camera_;Tab5Bluetooth bluetooth_;Audio audio_;Tab5Network network_;Usb usb_;Display display_;
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
