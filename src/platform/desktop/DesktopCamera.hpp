#pragma once
#include "platform/Platform.hpp"
namespace Pine {class DesktopCamera final:public CameraBackend{public:bool available()const override{return available_;}bool start()override{running_=available_;return running_;}void stop()override{running_=false;}bool capturePhoto(const std::filesystem::path&)override;void setAvailable(bool v)override{available_=v;if(!v)running_=false;}private:bool available_{true},running_{false};};}
