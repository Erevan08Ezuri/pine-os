cmake_minimum_required(VERSION 3.24)
project(PineOS VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
set(SDL_GPU OFF CACHE BOOL "" FORCE)
set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
FetchContent_Declare(SDL3 URL https://github.com/libsdl-org/SDL/releases/download/release-3.2.22/SDL3-3.2.22.tar.gz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(SDL3)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)
FetchContent_Declare(sqlite URL https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(sqlite)
add_library(sqlite3 STATIC "${sqlite_SOURCE_DIR}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${sqlite_SOURCE_DIR}")
if(MSVC)
  target_compile_definitions(sqlite3 PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
find_package(Threads REQUIRED)

configure_file(src/core/Version.hpp.in generated/Version.hpp @ONLY)

add_library(pine_core
  src/core/ApplicationManager.cpp src/core/Configuration.cpp src/core/Logger.cpp
  src/input/TextInputSession.cpp src/input/TextInputManager.cpp
  src/services/BatteryService.cpp src/services/BluetoothService.cpp src/services/CameraService.cpp
  src/services/AudioService.cpp src/services/NetworkService.cpp src/services/FileService.cpp
  src/services/UsbService.cpp src/services/DisplayService.cpp src/services/SystemService.cpp src/services/NotesService.cpp
  src/services/NotificationService.cpp src/services/SecurityService.cpp
  src/finance/Money.cpp src/finance/FinanceDatabase.cpp src/finance/FinanceService.cpp
  src/finance/AnalyticsEngine.cpp src/finance/ImportExportManager.cpp src/finance/FinanceSecurityManager.cpp
)
target_include_directories(pine_core PUBLIC src "${CMAKE_BINARY_DIR}/generated")
target_link_libraries(pine_core PUBLIC nlohmann_json::nlohmann_json Threads::Threads sqlite3)
if(MSVC)
  target_compile_options(pine_core PRIVATE /W4 /permissive- /utf-8)
else()
  target_compile_options(pine_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(pine_desktop_platform
  src/platform/PlatformFactory.cpp
  src/platform/desktop/DesktopPlatform.cpp src/platform/desktop/DesktopBattery.cpp
  src/platform/desktop/DesktopCamera.cpp src/platform/desktop/DesktopBluetooth.cpp
  src/platform/desktop/DesktopAudio.cpp src/platform/desktop/DesktopNetwork.cpp
  src/platform/desktop/DesktopUsb.cpp src/platform/desktop/DesktopDisplay.cpp
)
target_include_directories(pine_desktop_platform PUBLIC src)
target_link_libraries(pine_desktop_platform PUBLIC pine_core)

add_library(pine_ui
  src/ui/Font.cpp src/ui/Shell.cpp src/ui/HomeScreen.cpp src/ui/StatusBar.cpp
  src/ui/Navigation.cpp src/ui/DeveloperPanel.cpp
  src/input/OnScreenKeyboard.cpp
  src/apps/SettingsApp.cpp src/apps/FilesApp.cpp src/apps/CameraApp.cpp src/apps/BluetoothApp.cpp src/apps/NotesApp.cpp src/apps/FinanceApp.cpp
  src/apps/finance/FinanceViews.cpp src/apps/finance/FinanceWorkflows.cpp
)
target_include_directories(pine_ui PUBLIC src third_party)
target_link_libraries(pine_ui PUBLIC pine_core pine_desktop_platform SDL3::SDL3-static)

add_executable(pine src/main.cpp)
target_link_libraries(pine PRIVATE pine_ui)
add_custom_command(TARGET pine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:pine>/assets"
  COMMENT "Bundling fonts and visual assets")

include(CTest)
if(BUILD_TESTING)
  add_executable(pine_tests tests/native_tests.cpp)
  target_link_libraries(pine_tests PRIVATE pine_ui)
  add_test(NAME pine_services COMMAND pine_tests)
endif()

install(TARGETS pine RUNTIME DESTINATION bin)
install(DIRECTORY assets config DESTINATION share/pine-os)
