#include "esp_bsp_sdl.h"
#include "bsp/m5stack_tab5.h"
#include "bsp/touch.h"
#include "esp_lcd_touch_st7123.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_lcd_mipi_dsi.h"
extern esp_err_t bsp_display_new_with_handles_to_st7123(const bsp_display_config_t*,bsp_lcd_handles_t*);
static bsp_lcd_handles_t lcd;
static esp_lcd_touch_handle_t touch;
static SemaphoreHandle_t transfer_done;
static SemaphoreHandle_t refresh_done;
static void* frame_buffers[2];
static bool sitronix;
static bool signal_semaphore(SemaphoreHandle_t semaphore) {
    if(!semaphore)return false;
    // Color-copy completion can be invoked synchronously, while refresh
    // completion arrives from the display DMA ISR. Match the FreeRTOS API to
    // the actual call context instead of assuming every callback is an ISR.
    if(!xPortInIsrContext()){
        xSemaphoreGive(semaphore);
        return false;
    }
    BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(semaphore,&wake);
    return wake==pdTRUE;
}
static bool transfer_finished(esp_lcd_panel_handle_t panel,esp_lcd_dpi_panel_event_data_t* event,void* ctx) {
    return signal_semaphore(transfer_done);
}
static bool refresh_finished(esp_lcd_panel_handle_t panel,esp_lcd_dpi_panel_event_data_t* event,void* ctx) {
    return signal_semaphore(refresh_done);
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
        bsp_display_config_t config={0};
        err=sitronix?bsp_display_new_with_handles_to_st7123(&config,&lcd):bsp_display_new_with_handles(&config,&lcd);
        if(err!=ESP_OK) return err;
        transfer_done=xSemaphoreCreateBinary();
        refresh_done=xSemaphoreCreateBinary();
        if(!transfer_done||!refresh_done) return ESP_ERR_NO_MEM;
        esp_lcd_dpi_panel_event_callbacks_t callbacks={.on_color_trans_done=transfer_finished,.on_refresh_done=refresh_finished};
        err=esp_lcd_dpi_panel_register_event_callbacks(lcd.panel,&callbacks,NULL);
        if(err!=ESP_OK) return err;
        err=esp_lcd_dpi_panel_get_frame_buffer(lcd.panel,2,&frame_buffers[0],&frame_buffers[1]);
        if(err!=ESP_OK) return err;
    }
    *panel=lcd.panel;*io=lcd.io;return ESP_OK;
}
void* pine_tab5_framebuffer(int index){return index>=0&&index<2?frame_buffers[index]:NULL;}
esp_err_t pine_tab5_present(const void* pixels) {
    if(!lcd.panel||!pixels)return ESP_ERR_INVALID_STATE;
    while(xSemaphoreTake(refresh_done,0)==pdTRUE){}
    while(xSemaphoreTake(transfer_done,0)==pdTRUE){}
    esp_err_t err=esp_lcd_panel_draw_bitmap(lcd.panel,0,0,720,1280,pixels);
    if(err!=ESP_OK)return err;
    // When Pine renders directly into one of the LCD driver's framebuffers,
    // draw_bitmap changes the buffer selected for the *next* DMA refresh.
    // Waiting for refresh completion makes the old framebuffer safe to draw
    // into again, eliminating tearing/ghost images without a 1.8 MB copy.
    const bool direct=pixels==frame_buffers[0]||pixels==frame_buffers[1];
    if(direct)return xSemaphoreTake(refresh_done,pdMS_TO_TICKS(100))?ESP_OK:ESP_ERR_TIMEOUT;
    return xSemaphoreTake(transfer_done,pdMS_TO_TICKS(1000))?ESP_OK:ESP_ERR_TIMEOUT;
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
