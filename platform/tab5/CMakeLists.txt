cmake_minimum_required(VERSION 3.24)
set(IDF_TARGET esp32p4)
set(COMPONENTS main)
set(EXTRA_COMPONENT_DIRS
  "${CMAKE_CURRENT_LIST_DIR}/.deps/sdl/sdl"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/m5stack_tab5"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/esp_lcd_st7121"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/esp_sccb_intf"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/esp_cam_sensor"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/esp_ipa"
  "${CMAKE_CURRENT_LIST_DIR}/.deps/factory/platforms/tab5/components/esp_video")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(pine_tab5 VERSION 0.2.0)
