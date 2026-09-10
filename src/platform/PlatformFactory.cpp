#include "platform/Platform.hpp"
#include "platform/desktop/DesktopPlatform.hpp"
#include <stdexcept>
namespace Pine { std::unique_ptr<Platform> createPlatform(const std::string& target){if(target=="desktop")return std::make_unique<DesktopPlatform>();throw std::invalid_argument("Unsupported platform: "+target);} }
