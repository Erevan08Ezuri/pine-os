#include "platform/desktop/DesktopPlatform.hpp"
#include "core/Logger.hpp"
namespace Pine {DesktopPlatform::DesktopPlatform(){Logger::instance().info("PLATFORM","Desktop backend initialized");}DeviceInformation DesktopPlatform::deviceInfo()const{
#if defined(_M_X64)||defined(__x86_64__)
return{"Desktop Simulator","x86_64"};
#elif defined(_M_ARM64)||defined(__aarch64__)
return{"Desktop Simulator","arm64"};
#else
return{"Desktop Simulator","unknown"};
#endif
}}
