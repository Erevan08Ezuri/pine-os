get_filename_component(PINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${PINE_ROOT}/cmake/PineSources.cmake")
set(pine_sources ${PINE_CORE_SOURCES} ${PINE_UI_SOURCES})
list(TRANSFORM pine_sources PREPEND "${PINE_ROOT}/")
configure_file("${PINE_ROOT}/src/core/Version.hpp.in" "${CMAKE_CURRENT_BINARY_DIR}/Version.hpp" @ONLY)
idf_component_register(
  SRCS ${pine_sources} "tab5_main.cpp"
    "${PINE_ROOT}/src/platform/tab5/Tab5Platform.cpp"
    "${PINE_ROOT}/src/platform/tab5/Tab5Clock.cpp"
    "${PINE_ROOT}/src/platform/tab5/Tab5Bluetooth.cpp"
    "${PINE_ROOT}/src/platform/tab5/Tab5Camera.cpp"
  INCLUDE_DIRS "${PINE_ROOT}/src" "${PINE_ROOT}/third_party"
    "${CMAKE_CURRENT_BINARY_DIR}" "../.deps/json/single_include"
  REQUIRES sdl georgik__sdl_bsp pine_sqlite littlefs nvs_flash pthread
    m5stack_tab5 esp_lcd_touch esp_timer esp_system esp_wifi esp_wifi_remote esp_hosted bt esp_netif esp_event lwip esp_video esp_ipa)
target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_20)
target_compile_definitions(${COMPONENT_LIB} PUBLIC PINE_TAB5=1)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-error=missing-field-initializers -fstack-usage)
target_add_binary_data(${COMPONENT_LIB} "${PINE_ROOT}/assets/fonts/Inter-Regular.ttf" BINARY)
target_add_binary_data(${COMPONENT_LIB} "${PINE_ROOT}/assets/fonts/Inter-SemiBold.ttf" BINARY)
