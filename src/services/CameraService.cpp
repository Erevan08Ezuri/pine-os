#include "services/CameraService.hpp"
#include "core/Logger.hpp"
#include <chrono>
#include <format>
namespace Pine {
CameraService::CameraService(CameraBackend&b,std::filesystem::path storage):backend_(b),dcim_(std::move(storage)/"DCIM"){}
bool CameraService::available()const{return backend_.available();}
bool CameraService::start(){if(!backend_.start()){status_="No camera detected.";Logger::instance().warn("CAMERA",status_);return false;}status_="Camera ready";return true;}
void CameraService::stop(){backend_.stop();}
bool CameraService::previewFrame(CameraFrame& frame){if(!available()||!backend_.start())return false;if(!backend_.previewFrame(frame))return false;status_="Camera live";return true;}
std::filesystem::path CameraService::capturePhoto(){if(!available()){status_="No camera detected.";return{};}const auto name=std::format("PINE_{:%Y%m%d_%H%M%S}.bmp",std::chrono::system_clock::now());const auto path=dcim_/name;if(!backend_.capturePhoto(path)){status_="Capture failed";Logger::instance().error("CAMERA","Unable to save capture");return{};}status_="Saved "+name;Logger::instance().info("CAMERA",status_);return path;}
void CameraService::simulateAvailable(bool v){backend_.setAvailable(v);status_=v?"Camera ready":"No camera detected.";}
}
