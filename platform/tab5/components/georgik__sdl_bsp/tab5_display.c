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
static bool sitronix;
static bool transfer_finished(esp_lcd_panel_handle_t panel,esp_lcd_dpi_panel_event_data_t* event,void* ctx) {
    // IDF calls this from the DMA ISR, but CPU/direct framebuffer paths call
    // it synchronously from draw_bitmap. Use the matching FreeRTOS API.
    if(!xPortInIsrContext()) {
        xSemaphoreGive(transfer_done);
        return false;
    }
    BaseType_t wake=pdFALSE;
    xSemaphoreGiveFromISR(transfer_done,&wake);
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
        bsp_display_config_t config={0};
        err=sitronix?bsp_display_new_with_handles_to_st7123(&config,&lcd):bsp_display_new_with_handles(&config,&lcd);
        if(err!=ESP_OK) return err;
        transfer_done=xSemaphoreCreateBinary();
        if(!transfer_done) return ESP_ERR_NO_MEM;
        esp_lcd_dpi_panel_event_callbacks_t callbacks={.on_color_trans_done=transfer_finished};
        err=esp_lcd_dpi_panel_register_event_callbacks(lcd.panel,&callbacks,NULL);
        if(err!=ESP_OK) return err;
    }
    *panel=lcd.panel;*io=lcd.io;return ESP_OK;
}
esp_err_t pine_tab5_present(const void* pixels) {
    if(!lcd.panel||!transfer_done||!pixels)return ESP_ERR_INVALID_STATE;
    // A prior callback can leave this binary semaphore full. Drain that token
    // before submitting a new frame so the wait below always belongs to the
    // frame we just submitted. This prevents copy/scanout overlap from showing
    // the previous screen through the current one or producing visible tearing.
    while(xSemaphoreTake(transfer_done,0)==pdTRUE) {}
    esp_err_t err=esp_lcd_panel_draw_bitmap(lcd.panel,0,0,720,1280,pixels);
    if(err!=ESP_OK) return err;
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
