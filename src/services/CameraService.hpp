#pragma once
#include "platform/Platform.hpp"
#include <filesystem>
#include <string>
namespace Pine {class CameraService{public:CameraService(CameraBackend&,std::filesystem::path storage);bool available()const;bool start();void stop();std::filesystem::path capturePhoto();void simulateAvailable(bool);const std::string&lastStatus()const{return status_;}private:CameraBackend&backend_;std::filesystem::path dcim_;std::string status_{"Ready"};};}
