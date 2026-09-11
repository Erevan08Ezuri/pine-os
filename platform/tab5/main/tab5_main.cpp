#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include "platform/tab5/Tab5Platform.hpp"
#include "platform/tab5/Tab5Clock.hpp"
#include "core/Logger.hpp"
#include "esp_bsp_sdl.h"
#include "esp_littlefs.h"
#include "esp_partition.h"
#include "esp_pthread.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "bsp/m5stack_tab5.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <array>
#include <algorithm>
#include <stdexcept>
#include <memory>
extern const unsigned char regularStart[] asm("_binary_Inter_Regular_ttf_start");
extern const unsigned char regularEnd[] asm("_binary_Inter_Regular_ttf_end");
extern const unsigned char boldStart[] asm("_binary_Inter_SemiBold_ttf_start");
extern const unsigned char boldEnd[] asm("_binary_Inter_SemiBold_ttf_end");
namespace {
void logMemory(const char* stage){
    constexpr auto caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    ESP_LOGI("PineTab5","%s: internal free=%u largest=%u main stack remaining=%u bytes",
        stage,static_cast<unsigned>(heap_caps_get_free_size(caps)),
        static_cast<unsigned>(heap_caps_get_largest_free_block(caps)),
        static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
}
bool blankPartition(){
    auto* part=esp_partition_find_first(ESP_PARTITION_TYPE_DATA,ESP_PARTITION_SUBTYPE_ANY,"pine_data");
    if(!part)return false;
    std::array<unsigned char,4096> block;
    for(size_t offset=0;offset<part->size;offset+=block.size()){
        auto count=std::min(block.size(),static_cast<size_t>(part->size-offset));
        if(esp_partition_read(part,offset,block.data(),count)!=ESP_OK)return false;
        if(!std::all_of(block.begin(),block.begin()+count,[](auto b){return b==0xff;}))return false;
        vTaskDelay(1);
    }
    return true;
}
void mountStorage(){
    esp_vfs_littlefs_conf_t cfg{};cfg.base_path="/pine";cfg.partition_label="pine_data";cfg.format_if_mount_failed=false;
    auto result=esp_vfs_littlefs_register(&cfg);
    // Only blank flash may be provisioned automatically. Preserve corrupt data.
    if(result!=ESP_OK&&blankPartition()){
        ESP_ERROR_CHECK(esp_littlefs_format("pine_data"));result=esp_vfs_littlefs_register(&cfg);
    }
    ESP_ERROR_CHECK(result);
}
void pollTouch(Pine::Shell& shell){
    static esp_bsp_sdl_touch_info_t current{};static bool down=false;
    auto next=current;if(esp_bsp_sdl_touch_read(&next)!=ESP_OK)return;
    SDL_Event e{};
    if(next.pressed!=down){
        e.type=next.pressed?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button=SDL_BUTTON_LEFT;e.button.x=next.pressed?next.x:current.x;e.button.y=next.pressed?next.y:current.y;
        shell.handleEvent(e);
    }else if(next.pressed&&(next.x!=current.x||next.y!=current.y)){
        e.type=SDL_EVENT_MOUSE_MOTION;e.motion.x=next.x;e.motion.y=next.y;shell.handleEvent(e);
    }
    down=next.pressed;if(down)current=next;
}
}
extern "C" void app_main(){
    try{
        logMemory("app_main entered");
        ESP_ERROR_CHECK(nvs_flash_init());
        auto cfg=esp_pthread_get_default_config();cfg.stack_size=32768;cfg.inherit_cfg=true;
        ESP_ERROR_CHECK(esp_pthread_set_cfg(&cfg));
        mountStorage();
        Pine::Logger::instance().initialize("/pine/data",Pine::LogLevel::Info);
        Pine::Configuration config("/pine/data");config.load();config.settings().developerMode=false;
        if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());
        ESP_ERROR_CHECK(esp_bsp_sdl_touch_init());Pine::initializeTab5Clock();
        if(!Pine::initializeEmbeddedFonts(regularStart,regularEnd-regularStart,boldStart,boldEnd-boldStart))
            throw std::runtime_error("Bundled fonts unavailable");
        auto* surface=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565);
        if(!surface)throw std::runtime_error(SDL_GetError());
        auto* renderer=SDL_CreateSoftwareRenderer(surface);
        if(!renderer)throw std::runtime_error(SDL_GetError());
        {
            auto ownedShell=std::make_unique<Pine::Shell>(nullptr,renderer,config,Pine::createTab5Platform());
            auto& shell=*ownedShell;
            shell.textInput().setForceSoftwareKeyboard(true);
            logMemory("shell ready");
            auto previous=SDL_GetTicks();std::uint64_t lastClockSave=0;
            while(shell.running()){
                auto now=SDL_GetTicks();
                if(now-lastClockSave>=60000){Pine::persistTab5Clock();lastClockSave=now;logMemory("running");}
                pollTouch(shell);shell.update((now-previous)/1000.0);previous=now;shell.render();
                ESP_ERROR_CHECK(pine_tab5_present(surface->pixels));vTaskDelay(pdMS_TO_TICKS(10));
            }
        }
        Pine::shutdownFonts();SDL_DestroyRenderer(renderer);SDL_DestroySurface(surface);SDL_Quit();
    }catch(const std::exception& e){ESP_LOGE("PineTab5","Startup failed: %s",e.what());}
    for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
}
