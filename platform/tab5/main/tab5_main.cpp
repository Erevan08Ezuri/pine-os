#include "ui/Shell.hpp"
#include "ui/Font.hpp"
#include "platform/tab5/Tab5Platform.hpp"
#include "platform/tab5/Tab5Clock.hpp"
#include "platform/tab5/Tab5Splash.hpp"
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
#include <pthread.h>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <memory>

extern const unsigned char regularStart[] asm("_binary_Inter_Regular_ttf_start");
extern const unsigned char regularEnd[] asm("_binary_Inter_Regular_ttf_end");
extern const unsigned char boldStart[] asm("_binary_Inter_SemiBold_ttf_start");
extern const unsigned char boldEnd[] asm("_binary_Inter_SemiBold_ttf_end");
extern const unsigned char splashStart[] asm("_binary_pineos_gold_tab5_bmp_start");
extern const unsigned char splashEnd[] asm("_binary_pineos_gold_tab5_bmp_end");

namespace {
void logMemory(const char* stage){
    constexpr auto caps=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    ESP_LOGI("PineTab5","%s: internal free=%u largest=%u stack remaining=%u bytes",
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
    esp_vfs_littlefs_conf_t cfg{};
    cfg.base_path="/pine";
    cfg.partition_label="pine_data";
    cfg.format_if_mount_failed=false;
    auto result=esp_vfs_littlefs_register(&cfg);
    // Only blank flash may be provisioned automatically. Preserve corrupt data.
    if(result!=ESP_OK&&blankPartition()){
        ESP_ERROR_CHECK(esp_littlefs_format("pine_data"));
        result=esp_vfs_littlefs_register(&cfg);
    }
    ESP_ERROR_CHECK(result);
}

bool pollTouch(Pine::Shell& shell){
    static esp_bsp_sdl_touch_info_t current{};
    static bool down=false;
    auto next=current;
    if(esp_bsp_sdl_touch_read(&next)!=ESP_OK)return false;
    bool edge=false;
    SDL_Event e{};
    if(next.pressed!=down){
        e.type=next.pressed?SDL_EVENT_MOUSE_BUTTON_DOWN:SDL_EVENT_MOUSE_BUTTON_UP;
        e.button.button=SDL_BUTTON_LEFT;
        e.button.x=next.pressed?next.x:current.x;
        e.button.y=next.pressed?next.y:current.y;
        shell.handleEvent(e);
        edge=true;
    }else if(next.pressed&&(next.x!=current.x||next.y!=current.y)){
        e.type=SDL_EVENT_MOUSE_MOTION;
        e.motion.x=next.x;
        e.motion.y=next.y;
        shell.handleEvent(e);
    }
    down=next.pressed;
    if(down)current=next;
    return edge;
}

void* pineMainThread(void*){
    try{
        logMemory("pine pthread entered");
        // SDL3 uses pthread APIs internally on ESP-IDF. Running PineOS from a
        // pthread-backed task ensures pthread_self() has a valid thread ID.
        if(!SDL_Init(SDL_INIT_VIDEO))throw std::runtime_error(SDL_GetError());

        if(!Pine::initializeEmbeddedFonts(regularStart,regularEnd-regularStart,boldStart,boldEnd-boldStart))
            throw std::runtime_error("Bundled fonts unavailable");

        void* lcdBuffers[2]={pine_tab5_framebuffer(0),pine_tab5_framebuffer(1)};
        const bool directBuffers=lcdBuffers[0]&&lcdBuffers[1];
        SDL_Surface* surfaces[2]{};
        SDL_Renderer* renderers[2]{};
        if(directBuffers){
            for(int i=0;i<2;++i){
                surfaces[i]=SDL_CreateSurfaceFrom(720,1280,SDL_PIXELFORMAT_RGB565,lcdBuffers[i],720*2);
                if(!surfaces[i])throw std::runtime_error(SDL_GetError());
                renderers[i]=SDL_CreateSoftwareRenderer(surfaces[i]);
                if(!renderers[i])throw std::runtime_error(SDL_GetError());
            }
            ESP_LOGI("PineTab5","Direct double-buffered LCD rendering enabled");
        }else{
            surfaces[0]=SDL_CreateSurface(720,1280,SDL_PIXELFORMAT_RGB565);
            if(!surfaces[0])throw std::runtime_error(SDL_GetError());
            renderers[0]=SDL_CreateSoftwareRenderer(surfaces[0]);
            if(!renderers[0])throw std::runtime_error(SDL_GetError());
            ESP_LOGW("PineTab5","LCD double buffers unavailable; using copy fallback");
        }

        {
            int backBuffer=directBuffers?1:0; // DSI starts scanning framebuffer 0.
            auto* splashIo=SDL_IOFromConstMem(splashStart,splashEnd-splashStart);
            if(!splashIo)throw std::runtime_error(SDL_GetError());
            std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)> artwork(
                SDL_LoadBMP_IO(splashIo,true),SDL_DestroySurface);
            Pine::Tab5Splash splash(artwork.get());
            auto showProgress=[&](float progress){
                splash.render(renderers[backBuffer],surfaces[backBuffer],progress);
                ESP_ERROR_CHECK(pine_tab5_present(surfaces[backBuffer]->pixels));
                if(directBuffers)backBuffer^=1;
            };
            // Milestones represent completed initialization work, not a timer.
            // Show branding before mounting storage (first boot may format it).
            showProgress(0.10f);
            mountStorage();
            Pine::Logger::instance().initialize("/pine/data",Pine::LogLevel::Info);
            showProgress(0.30f);
            Pine::Configuration config("/pine/data");
            config.load();
            config.settings().developerMode=false;
            showProgress(0.45f);
            ESP_ERROR_CHECK(esp_bsp_sdl_touch_init());
            showProgress(0.55f);
            Pine::initializeTab5Clock();
            showProgress(0.65f);
            auto platform=Pine::createTab5Platform();
            showProgress(0.80f);
            auto ownedShell=std::make_unique<Pine::Shell>(nullptr,renderers[backBuffer],config,std::move(platform));
            auto& shell=*ownedShell;
            shell.textInput().setForceSoftwareKeyboard(true);
            showProgress(1.0f);
            vTaskDelay(pdMS_TO_TICKS(150)); // Briefly present completion before home.
            artwork.reset(); // Release splash RAM before the interactive loop.
            shell.completeBoot(); // The Tab5 has already shown its real boot flow.
            logMemory("shell ready");
            auto previous=SDL_GetTicks();
            auto lastPresent=previous;
            std::uint64_t lastClockSave=0;
            constexpr std::uint64_t fallbackFrameIntervalMs=33;
            constexpr std::uint64_t interactionFrameMinMs=8;
            while(shell.running()){
                auto now=SDL_GetTicks();
                if(now-lastClockSave>=60000){
                    Pine::persistTab5Clock();
                    lastClockSave=now;
                    logMemory("running");
                }
                const bool touchEdge=pollTouch(shell);
                shell.update((now-previous)/1000.0);
                previous=now;

                if(directBuffers){
                    // Render only into the framebuffer that is not currently
                    // being scanned out. pine_tab5_present waits for the refresh
                    // boundary before this buffer can become the front buffer.
                    shell.setRenderer(renderers[backBuffer]);
                    shell.render();
                    ESP_ERROR_CHECK(pine_tab5_present(lcdBuffers[backBuffer]));
                    backBuffer^=1;
                    lastPresent=SDL_GetTicks();
                }else{
                    const bool scheduledFrame=now-lastPresent>=fallbackFrameIntervalMs;
                    const bool interactionFrame=touchEdge&&now-lastPresent>=interactionFrameMinMs;
                    if(scheduledFrame||interactionFrame){
                        shell.render();
                        ESP_ERROR_CHECK(pine_tab5_present(surfaces[0]->pixels));
                        lastPresent=SDL_GetTicks();
                    }
                    vTaskDelay(pdMS_TO_TICKS(2));
                }
            }
        }

        Pine::shutdownFonts();
        for(int i=0;i<2;++i){
            if(renderers[i])SDL_DestroyRenderer(renderers[i]);
            if(surfaces[i])SDL_DestroySurface(surfaces[i]);
        }
        SDL_Quit();
    }catch(const std::exception& e){
        ESP_LOGE("PineTab5","Startup failed: %s",e.what());
    }

    for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
    return nullptr;
}
}

extern "C" void app_main(){
    logMemory("app_main entered");
    ESP_ERROR_CHECK(nvs_flash_init());

    auto cfg=esp_pthread_get_default_config();
    cfg.stack_size=32768;
    cfg.inherit_cfg=true;
    ESP_ERROR_CHECK(esp_pthread_set_cfg(&cfg));

    pthread_t pineThread{};
    const int rc=pthread_create(&pineThread,nullptr,pineMainThread,nullptr);
    if(rc!=0){
        ESP_LOGE("PineTab5","Failed to create PineOS pthread: %d",rc);
        for(;;)vTaskDelay(pdMS_TO_TICKS(1000));
    }

    pthread_detach(pineThread);
    ESP_LOGI("PineTab5","PineOS pthread started");
}
