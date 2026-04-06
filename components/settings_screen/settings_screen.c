#include "ili9341.h"
#include "settings_screen.h"
#include "animated.h"
#include "nvs_flash.h"
#include <stdint.h>
#include "define.h"
#include <stdio.h>
#include <string.h>

extern uint8_t curr_screen;
extern uint8_t key1Status, key2Status, key3Status;

#define FOCUS_SETTINGS_SCREEN 5 // Updated to match define.h

#define STACK_SIZE_DrawSettings 3000
StaticTask_t xTaskBuffer_DrawSettings;
StackType_t xStack_DrawSettings[ STACK_SIZE_DrawSettings ];

#define STACK_SIZE_SettingsKey 2000
StaticTask_t xTaskBuffer_SettingsKey;
StackType_t xStack_SettingsKey[ STACK_SIZE_SettingsKey ];

TaskHandle_t xHandle_DrawSettings;
TaskHandle_t xHandle_SettingsKey;

nvs_handle_t S_nvs_handle;
uint32_t alarm_volume = 80; // Default 80%

void DrawSettingsList();
void SettingsKeyEvent();
void save_settings();

void init_settings(){
    curr_screen = SETTINGS_SCREEN;
    fill_display(&bg_color);
    
    esp_err_t err = nvs_open("Settings", NVS_READWRITE, &S_nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        // Load volume
        err = nvs_get_u32(S_nvs_handle, "alarm_volume", &alarm_volume);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            alarm_volume = 80;
            nvs_set_u32(S_nvs_handle, "alarm_volume", alarm_volume);
            nvs_commit(S_nvs_handle);
        }
    }

    show_text((uint8_t*)"Settings", &text_clock, &bg_color, 110, 5, char_map30, font_pixels30);

    xHandle_DrawSettings = xTaskCreateStatic(
                     DrawSettingsList, "Draw Settings",
                     STACK_SIZE_DrawSettings, NULL, 
                     1, xStack_DrawSettings, &xTaskBuffer_DrawSettings );
    configASSERT( xHandle_DrawSettings );

    xHandle_SettingsKey = xTaskCreateStatic(
                     SettingsKeyEvent, "Settings Key",
                     STACK_SIZE_SettingsKey, NULL, 
                     2, xStack_SettingsKey, &xTaskBuffer_SettingsKey );
    configASSERT( xHandle_SettingsKey );
}

void stop_settings(){
    nvs_close(S_nvs_handle);
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    if(xHandle_DrawSettings) vTaskDelete( xHandle_DrawSettings );
    if(xHandle_SettingsKey) vTaskDelete( xHandle_SettingsKey );
    xSemaphoreGive(spi_xSemaphore);
}

uint32_t get_alarm_volume() {
    // If NVS is not open, we might need to open it or use a cached value.
    // Since this might be called from interrupt or other tasks, better to use a cached value.
    // But we need to ensure it's initialized.
    // For now, return the static variable.
    return alarm_volume;
}

void save_settings() {
    nvs_set_u32(S_nvs_handle, "alarm_volume", alarm_volume);
    nvs_commit(S_nvs_handle);
}

void SettingsKeyEvent(){
    while(1){
        if(!key1Status && !key2Status && !key3Status){
            vTaskDelay(30 / portTICK_PERIOD_MS);
            continue;
        }

        if(curr_screen == SETTINGS_SCREEN){
            if(key3Status){ // Enter Edit Mode
                curr_screen = FOCUS_SETTINGS_SCREEN;
                key3Status = 0;
            }
            // Key 1 and Key 2 are left for navigation (handled by main.c for exit, or future list nav)
        } else if(curr_screen == FOCUS_SETTINGS_SCREEN){
            if(key3Status){ // Exit Edit Mode
                curr_screen = SETTINGS_SCREEN;
                save_settings();
                key3Status = 0;
            } else if(key1Status){ // Decrease
                if(alarm_volume > 0) alarm_volume -= 10;
                key1Status = 0;
            } else if(key2Status){ // Increase
                if(alarm_volume < 100) alarm_volume += 10;
                key2Status = 0;
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS); // Debounce/Repeat rate
    }
}

void show_text_settings(const uint8_t *str,
                    color_struct *font_color, color_struct *bg_color, uint16_t Xs, uint16_t Ys,
                    const struct font_char char_map[], const uint8_t font_pixels[]){
    uint8_t v;
    struct font_char font_c;
    color_struct last_color;
    uint16_t last_v = 256;
    uint16_t Xe, Ye;
    float alpha, oneminusalpha;
    while(*str != '\0') {
        font_c = char_map[*str];
        Xe = Xs + font_c.advance;
        Ye = Ys + font_c.h + font_c.top;
        for(int y = Ys; y < char_map[0].h + Ys; y++){
            for(int x = Xs; x < Xe; x++){
                write_buffer(lcd_buffer, x, y, *bg_color);
            }
        }
        for(int y = Ys + font_c.top; y < Ye; y++){
            for(int x = Xs + font_c.left; x < Xe; x++){
                v = font_pixels[font_c.offset + (x - Xs - font_c.left) + (y - Ys - font_c.top) * font_c.w];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((font_color->r * alpha) + (oneminusalpha * bg_color->r));
                    last_color.g = (uint8_t)((font_color->g * alpha) + (oneminusalpha * bg_color->g));
                    last_color.b = (uint8_t)((font_color->b * alpha) + (oneminusalpha * bg_color->b));
                    last_v = v;
                }
                write_buffer(lcd_buffer, x, y, last_color);
            }
        }
        Xs = Xs + font_c.advance;
        str++;
    };
}

void DrawSettingsList(){
    char vol_str[20];
    uint32_t last_volume = 200; // Force update
    uint8_t last_focus = 255;

    while(1){
        bool focus = (curr_screen == FOCUS_SETTINGS_SCREEN);
        
        if(last_volume != alarm_volume || last_focus != focus){
            xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
            
            // Clear line (simple fill)
            // Assuming item is at y=60
            for(int y=60; y<100; y++){
                for(int x=0; x<320; x++){
                    write_buffer(lcd_buffer, x, y, bg_color);
                }
            }

            color_struct *color = focus ? &cyan_color : &white_color;
            
            sprintf(vol_str, "Volume: %ld%%", alarm_volume);
            show_text_settings((uint8_t*)vol_str, color, &bg_color, 20, 60, char_map30, font_pixels30);

            // Draw a simple bar
            // Background bar
            for(int i=0; i<200; i++){
                for(int j=0; j<10; j++){
                    write_buffer(lcd_buffer, 60+i, 95+j, grey_color);
                }
            }
            // Fill bar
            int fill_width = (alarm_volume * 200) / 100;
            for(int i=0; i<fill_width; i++){
                for(int j=0; j<10; j++){
                    write_buffer(lcd_buffer, 60+i, 95+j, green_color);
                }
            }

            xSemaphoreGive(spi_xSemaphore);
            
            last_volume = alarm_volume;
            last_focus = focus;
        }
        
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void redraw_settings_static() {
    show_text((uint8_t*)"Settings", &text_clock, &bg_color, 110, 5, char_map30, font_pixels30);
    // Force redraw of list in next loop of DrawSettingsList by resetting last_volume?
    // Or just let it be.
}
