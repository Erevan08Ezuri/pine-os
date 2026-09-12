#include "esp_bsp_sdl.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/touch.h"
#include "esp_lcd_touch_st7123.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_mipi_dsi.h"
#include <string.h>
extern esp_err_t bsp_display_new_with_handles_to_st7123(const bsp_display_config_t*,bsp_lcd_handles_t*);
static bsp_lcd_handles_t lcd;
static esp_lcd_touch_handle_t touch;
static SemaphoreHandle_t frame_complete;
static void* frame_buffers[2];
static size_t back_buffer_index=1;
static size_t panel_width=720;
static bool sitronix;
static bool frame_finished(esp_lcd_panel_handle_t panel,esp_lcd_dpi_panel_event_data_t* event,void* ctx) {
    // IDF normally invokes this from the DSI DMA ISR. Keep the synchronous path
    // valid as well so the shim is safe across compatible ESP-IDF patch releases.
    if(!xPortInIsrContext()) {
        xSemaphoreGive(frame_complete);
        return false;
    }
    BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(frame_complete,&wake);
    return wake==pdTRUE;
}
esp_err_t esp_bsp_sdl_init(esp_bsp_sdl_display_config_t* cfg,esp_lcd_panel_handle_t* panel,esp_lcd_panel_io_handle_t* io) {
    // Pine handles touch directly: upstream SDL incorrectly uses pixel coordinates
    // in its normalized touch API. Keep the native portrait coordinate system.
    *cfg=(esp_bsp_sdl_display_config_t){720,1280,0x15151002u,720*1280*2,false};
    if(!lcd.panel) {
        esp_err_t err=bsp_i2c_init();
        if(err!=ESP_OK) return err;
        bsp_io_expander_pi4ioe_init(bsp_i2c_get_handle());
        bsp_reset_tp();
        sitronix=i2c_master_probe(bsp_i2c_get_handle(),0x55,100)==ESP_OK;
        panel_width=sitronix?800:720;
        bsp_display_config_t config={0};
        err=sitronix?bsp_display_new_with_handles_to_st7123(&config,&lcd):bsp_display_new_with_handles(&config,&lcd);
        if(err!=ESP_OK) return err;

        // bsp_display_new_with_handles() does not initialize the PWM backlight.
        // Without this, Settings changes the stored value but not the actual LCD.
        err=bsp_display_brightness_init();
        if(err!=ESP_OK) return err;

        // PineOS page-flips between two BSP-owned DSI frame buffers. This keeps
        // the LCD from scanning a framebuffer while Pine is rewriting it, which
        // removes the old-screen bleed-through, tearing and full-screen flicker.
        err=esp_lcd_dpi_panel_get_frame_buffer(lcd.panel,2,&frame_buffers[0],&frame_buffers[1]);
        if(err!=ESP_OK) return err;
        frame_complete=xSemaphoreCreateBinary();
        if(!frame_complete) return ESP_ERR_NO_MEM;
        esp_lcd_dpi_panel_event_callbacks_t callbacks={.on_frame_buf_complete=frame_finished};
        err=esp_lcd_dpi_panel_register_event_callbacks(lcd.panel,&callbacks,NULL);
        if(err!=ESP_OK) return err;
    }
    *panel=lcd.panel;*io=lcd.io;return ESP_OK;
}
esp_err_t pine_tab5_present(const void* pixels) {
    if(!lcd.panel||!pixels||!frame_buffers[0]||!frame_buffers[1]||!frame_complete)return ESP_ERR_INVALID_STATE;

    // Drain a completion token left by an idle refresh. The wait below must
    // correspond to the page flip scheduled by this call.
    while(xSemaphoreTake(frame_complete,0)==pdTRUE) {}

    auto* target=(unsigned char*)frame_buffers[back_buffer_index];
    const auto* source=(const unsigned char*)pixels;
    constexpr size_t source_width=720;
    constexpr size_t height=1280;
    constexpr size_t bytes_per_pixel=2;
    for(size_t y=0;y<height;++y){
        memcpy(target+y*panel_width*bytes_per_pixel,
               source+y*source_width*bytes_per_pixel,
               source_width*bytes_per_pixel);
    }

    // Because target is one of the DPI driver's own framebuffers, draw_bitmap
    // selects it for the next scanout instead of copying into the live buffer.
    esp_err_t err=esp_lcd_panel_draw_bitmap(lcd.panel,0,0,720,1280,target);
    if(err!=ESP_OK) return err;
    if(xSemaphoreTake(frame_complete,pdMS_TO_TICKS(1000))!=pdTRUE)return ESP_ERR_TIMEOUT;

    back_buffer_index^=1;
    return ESP_OK;
}
esp_err_t esp_bsp_sdl_backlight_on(void){return bsp_display_backlight_on();}
esp_err_t esp_bsp_sdl_backlight_off(void){return bsp_display_backlight_off();}
esp_err_t esp_bsp_sdl_display_on_off(bool on){return esp_lcd_panel_disp_on_off(lcd.panel,on);}
esp_err_t esp_bsp_sdl_touch_init(void) {
    if(touch) return ESP_OK;
    if(!sitronix) {
        bsp_touch_config_t cfg={0};
        esp_err_t err=bsp_touch_new(&cfg,&touch);
        if(err==ESP_OK) esp_lcd_touch_exit_sleep(touch);
        return err;
    }
    esp_lcd_panel_io_handle_t io=NULL;
    esp_lcd_panel_io_i2c_config_t io_cfg=ESP_LCD_TOUCH_IO_I2C_ST7123_CONFIG();
    io_cfg.scl_speed_hz=400000;
    esp_err_t err=esp_lcd_new_panel_io_i2c(bsp_i2c_get_handle(),&io_cfg,&io);
    if(err!=ESP_OK) return err;
    const esp_lcd_touch_config_t cfg={.x_max=720,.y_max=1280,.rst_gpio_num=-1,.int_gpio_num=23};
    return esp_lcd_touch_new_i2c_st7123(io,&cfg,&touch);
}
esp_err_t esp_bsp_sdl_touch_read(esp_bsp_sdl_touch_info_t* info) {
    if(!touch||!info) return ESP_ERR_INVALID_STATE;
    esp_err_t err=esp_lcd_touch_read_data(touch);
    if(err!=ESP_OK) return err;
    uint16_t x=0,y=0,strength=0;uint8_t count=0;
    info->pressed=esp_lcd_touch_get_coordinates(touch,&x,&y,&strength,&count,1)&&count;
    if(info->pressed){info->x=x<720?x:719;info->y=y<1280?y:1279;}
    return ESP_OK;
}
const char* esp_bsp_sdl_get_board_name(void){return "M5Stack Tab5";}
