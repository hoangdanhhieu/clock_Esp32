#include "ili9341.h"
#include "alarm_screen.h"
#include "animated.h"
#include "nvs_flash.h"
#include <stdint.h>
#include "define.h"
#include <math.h>
#define FOCUS_ALARM_SCREEN 2
#define NOT_EDIT 0
#define EDIT_HOUR 1
#define EDIT_MINUTE 2

#define STACK_SIZE_DrawList 3000
StaticTask_t xTaskBuffer_DrawList;
StackType_t xStack_DrawList[ STACK_SIZE_DrawList ];

#define STACK_SIZE_KeyEvent 2000
StaticTask_t xTaskBuffer_KeyEvent;
StackType_t xStack_KeyEvent[ STACK_SIZE_KeyEvent ];

TaskHandle_t xHandle_DrawList;
TaskHandle_t xHandle_KeyEvent;

uint8_t s, Aindex = 1;
nvs_handle_t A_nvs_handle;

uint8_t reload;
uint8_t editAlarm;

const int border_y[] = {50, 102, 154, 206};
const int time_y[] = {60, 112, 164};
const int sw_t[] = {52, 104, 156};

color_struct get_pixel_color(int x, int y) {
    int index = (y * 320 + x) * 2;
    uint8_t high = lcd_buffer[index];
    uint8_t low = lcd_buffer[index + 1];
    color_struct c;
    c.r = (high >> 3) & 0x1F;
    c.g = ((high & 0x07) << 3) | (low >> 5);
    c.b = low & 0x1F;
    return c;
}

// Integer blending macro for 5-bit and 6-bit channels
// alpha is 0-255
#define BLEND(fg, bg, alpha) ( ( (fg) * (alpha) + (bg) * (255 - (alpha)) ) / 255 )

void show_text_transparent(const uint8_t *str,
                    color_struct *font_color, uint16_t Xs, uint16_t Ys,
                    const struct font_char char_map[], const uint8_t font_pixels[]){
    uint8_t v;
    struct font_char font_c;
    color_struct last_color;
    uint16_t Xe, Ye;
    
    // Pre-calculate high/low bytes for background reading if needed
    // But we read pixel by pixel, so we can't pre-calc much for BG.
    
    while(*str != '\0') {
        font_c = char_map[*str];
        Xe = Xs + font_c.advance;
        Ye = Ys + font_c.h + font_c.top;
        
        // Optimization: Check bounds to avoid drawing outside screen
        if (Xs < 320 && Ys < 240) {
            for(int y = Ys + font_c.top; y < Ye; y++){
                if (y >= 240) break;
                int y_offset = y * 320;
                int font_y_offset = (y - Ys - font_c.top) * font_c.w;
                
                for(int x = Xs + font_c.left; x < Xe; x++){
                    if (x >= 320) break;
                    
                    v = font_pixels[font_c.offset + (x - Xs - font_c.left) + font_y_offset];
                    
                    if(v != 0){ 
                        // Inline get_pixel_color for speed
                        int index = (y_offset + x) * 2;
                        uint8_t high = lcd_buffer[index];
                        uint8_t low = lcd_buffer[index + 1];
                        
                        // Extract RGB (5-6-5)
                        uint8_t bg_r = (high >> 3) & 0x1F;
                        uint8_t bg_g = ((high & 0x07) << 3) | (low >> 5);
                        uint8_t bg_b = low & 0x1F;

                        if (v == 255) {
                            // Opaque pixel - no blending needed
                            last_color = *font_color;
                        } else {
                            // Integer blending
                            last_color.r = BLEND(font_color->r, bg_r, v);
                            last_color.g = BLEND(font_color->g, bg_g, v);
                            last_color.b = BLEND(font_color->b, bg_b, v);
                        }
                        
                        // Inline write_buffer
                        lcd_buffer[index] = (last_color.r << 3) | (last_color.g >> 3);
                        lcd_buffer[index + 1] = (last_color.g << 5) | last_color.b;
                    }
                }
            }
        }
        Xs = Xs + font_c.advance;
        str++;
    };
}

void IRAM_ATTR drawSwitch_transparent(int frame, int i){
    uint8_t v;
    color_struct last_color;
    
    int start_y = sw_t[i];
    int end_y = start_y + 50;
    
    for(int y = start_y; y < end_y; y++){
        int y_offset = y * 320;
        int switch_y_offset = (y - start_y) * 50;
        int frame_offset = 2500 * frame;
        
        for(int x = 260; x < 310; x++){
            // switchh is likely inverted or something? Original code: 255 - switchh[...]
            v = 255 - switchh[frame_offset + switch_y_offset + (x - 260)];
            
            if(v != 0){ 
                int index = (y_offset + x) * 2;
                uint8_t high = lcd_buffer[index];
                uint8_t low = lcd_buffer[index + 1];
                
                uint8_t bg_r = (high >> 3) & 0x1F;
                uint8_t bg_g = ((high & 0x07) << 3) | (low >> 5);
                uint8_t bg_b = low & 0x1F;

                if (v == 255) {
                    last_color = switch_color;
                } else {
                    last_color.r = BLEND(switch_color.r, bg_r, v);
                    last_color.g = BLEND(switch_color.g, bg_g, v);
                    last_color.b = BLEND(switch_color.b, bg_b, v);
                }
                
                lcd_buffer[index] = (last_color.r << 3) | (last_color.g >> 3);
                lcd_buffer[index + 1] = (last_color.g << 5) | last_color.b;
            }
        }
    }
}

void DrawList();
void KeyEvent();
void update_alarmNVS(int A);
void reverse_alarm(int A);
void drawSwitch(int frame, int i, const color_struct bg_rgb);
void animate_selection(int i, uint8_t is_selecting);

void draw_rounded_rect(int x, int y, int w, int h, int r, color_struct color) {
    // Fill center block
    for (int j = r; j < h - r; j++) {
        for (int i = 0; i < w; i++) {
            write_buffer(lcd_buffer, x + i, y + j, color);
        }
    }
    // Fill top and bottom blocks (excluding corners)
    for (int j = 0; j < r; j++) {
        for (int i = r; i < w - r; i++) {
            write_buffer(lcd_buffer, x + i, y + j, color);
            write_buffer(lcd_buffer, x + i, y + h - 1 - j, color);
        }
    }
    // Draw corners
    for (int j = 0; j < r; j++) {
        for (int i = 0; i < r; i++) {
            if ((r - i - 1) * (r - i - 1) + (r - j - 1) * (r - j - 1) <= r * r) {
                write_buffer(lcd_buffer, x + i, y + j, color);                 // Top-left
                write_buffer(lcd_buffer, x + w - 1 - i, y + j, color);         // Top-right
                write_buffer(lcd_buffer, x + i, y + h - 1 - j, color);         // Bottom-left
                write_buffer(lcd_buffer, x + w - 1 - i, y + h - 1 - j, color); // Bottom-right
            }
        }
    }
}

void init_alarm(){
    curr_screen = ALARM_SCREEN;
    fill_display(&bg_color);
    esp_err_t err;
    err = nvs_open("Alarm", NVS_READWRITE, &A_nvs_handle);
    Aindex = 1;
    reload = 0;
    editAlarm = NOT_EDIT;
    if (err != ESP_OK) {
        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } else {
        printf("Done\n");
    }
    show_text((uint8_t*)"Alarm", &text_clock, &bg_color, 130, 5, char_map30, font_pixels30);
    xHandle_DrawList = xTaskCreateStatic(
                     DrawList, "Draw DrawList",
                     STACK_SIZE_DrawList,  ( void * ) 3, 
                     1, xStack_DrawList, &xTaskBuffer_DrawList );
    configASSERT( xHandle_DrawList );

    xHandle_KeyEvent = xTaskCreateStatic(
                     KeyEvent, "Draw KeyEvent",
                     STACK_SIZE_KeyEvent, NULL, 
                     2, xStack_KeyEvent, &xTaskBuffer_KeyEvent );
    configASSERT( xHandle_KeyEvent );
}

void stop_alarm(){
    nvs_close(A_nvs_handle);
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    vTaskDelete( xHandle_DrawList );
    vTaskDelete( xHandle_KeyEvent );
    xSemaphoreGive(spi_xSemaphore);
}

void KeyEvent(){
    uint32_t Astatus, hour, minute;
    while(1){
        if(!key1Status && !key2Status && !key3Status){
            vTaskDelay(30 / portTICK_PERIOD_MS);
            reload = 0;
            continue;
        }
        if(curr_screen == ALARM_SCREEN){
            if(key3Status){
                curr_screen = FOCUS_ALARM_SCREEN;
                Aindex = 0;
                reload = 1;
                editAlarm = NOT_EDIT;
            }
        } else if(curr_screen == FOCUS_ALARM_SCREEN){
            if(key3Status == CLICK_KEY){
                if(editAlarm == EDIT_HOUR){
                    editAlarm = EDIT_MINUTE;
                } else if(editAlarm == EDIT_MINUTE){
                    editAlarm = NOT_EDIT;
                    update_alarmNVS(Aindex);  // Save BEFORE resetting Aindex!
                    Aindex = 0;
                    curr_screen = ALARM_SCREEN;
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                } else {
                    editAlarm = EDIT_HOUR;
                }
            } else if(key3Status == HOLD_KEY){
                reverse_alarm(Aindex);
            } else if(editAlarm){
                Astatus = (alarm_array[Aindex] >= 100000) ? 100000 : 0;
                hour = (alarm_array[Aindex] - Astatus) / 60;
                minute = (alarm_array[Aindex] - Astatus) % 60;
                if(key1Status){
                    while(key1Status){
                        if(editAlarm == EDIT_HOUR){
                            hour = (hour + 23) % 24;
                        } else {
                            minute = (minute + 59) % 60;
                        }
                        key1Status = NOTHING;
                        alarm_array[Aindex] = Astatus + hour * 60 + minute;
                        reload = 1;
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                        
                    }
                } else {
                    while(key2Status){
                        if(editAlarm == EDIT_HOUR){
                            hour = (hour + 1) % 24;
                        } else {
                            minute = (minute + 1) % 60;
                        }
                        alarm_array[Aindex] = Astatus + hour * 60 + minute;
                        reload = 1;
                        key2Status = NOTHING;
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                    }
                }
            } else {
                if(key2Status){
                    if(Aindex == 9){
                        curr_screen = ALARM_SCREEN;
                    } else {
                        Aindex++;
                    }
                } else {
                    if(Aindex == 0){
                        curr_screen = ALARM_SCREEN;
                    } else {
                        Aindex--;
                    }
                }
            }
            reload = 1;
            key1Status = key2Status = 0;
        }
        key3Status = 0;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void reverse_alarm(int A){
    if(alarm_array[A] >= 100000){
        for(int f = 0; f < 14; f++){
            drawSwitch(f, 2 - s + Aindex, select_bg);
            vTaskDelay(14 / portTICK_PERIOD_MS);
        }
        alarm_array[A]-=100000;
    } else {
        for(int f = 14; f < 27; f++){
            drawSwitch(f, 2 - s + Aindex, select_bg);
            vTaskDelay(14 / portTICK_PERIOD_MS);
        }
        alarm_array[A]+=100000;
    }
    update_alarmNVS(A);  // Save alarm state to NVS
}

void update_alarmNVS(int A){
    esp_err_t err;
    err = nvs_set_u32(A_nvs_handle, name_alarm[A], alarm_array[A]);
    if (err != ESP_OK) {
        printf("Error (%s) setting alarm %d in NVS!\n", esp_err_to_name(err), A);
        return;
    }
    
    err = nvs_commit(A_nvs_handle);
    if (err != ESP_OK) {
        printf("Error (%s) committing alarm %d to NVS!\n", esp_err_to_name(err), A);
    } else {
        printf("Alarm %d saved to NVS successfully\n", A);
    }
}


void IRAM_ATTR drawSwitch(int frame, int i, const color_struct bg_color){
    float alpha, oneminusalpha;
    uint8_t v;
    color_struct last_color;
    uint16_t last_v = 256;
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    for(int y = sw_t[i]; y < sw_t[i] + 50; y++){
        for(int x = 260; x < 310; x++){
            v = 255 - switchh[2500 * frame + (y - sw_t[i]) * 50 + (x - 260)];
            if(last_v != v){
                alpha = (float)v / 255;
                oneminusalpha = 1 - alpha;
                last_color.r = (uint8_t)((switch_color.r * alpha) + (oneminusalpha * bg_color.r));
                last_color.g = (uint8_t)((switch_color.g * alpha) + (oneminusalpha * bg_color.g));
                last_color.b = (uint8_t)((switch_color.b * alpha) + (oneminusalpha * bg_color.b));
                last_v = v;
            }
            write_buffer(lcd_buffer, x, y, last_color);
        }
    }
    xSemaphoreGive(spi_xSemaphore);
}

// Brightness pulse animation for alarm selection
void animate_selection(int i, uint8_t is_selecting) {
    const int y_start = border_y[i] + 2;
    const int y_end = border_y[i] + 52;
    
    if(is_selecting) {
        // Quick brightness pulse - add white light then fade back
        uint8_t white_add[] = {80, 60, 40, 20, 10, 0};  // 6 frames, add white then fade
        
        for(int frame = 0; frame < 6; frame++) {
            color_struct pulse_color;
            uint16_t r = select_bg.r + white_add[frame];
            uint16_t g = select_bg.g + white_add[frame];
            uint16_t b = select_bg.b + white_add[frame];
            pulse_color.r = (r > 255) ? 255 : r;
            pulse_color.g = (g > 255) ? 255 : g;
            pulse_color.b = (b > 255) ? 255 : b;
            
            xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
            // Fill only the text area, not the switch area (preserve switch at x=260-310)
            for(int y = y_start; y < y_end; y++){
                for(int x = 0; x < 255; x++){  // Stop before switch area
                    write_buffer(lcd_buffer, x, y, pulse_color);
                }
            }
            xSemaphoreGive(spi_xSemaphore);
            vTaskDelay(40 / portTICK_PERIOD_MS);
        }
    }
}

void DrawList(){
    s = 2;
    int t;
    uint8_t Hourbuffer[11];
    uint8_t Minutebuffer[11];
    
    float current_y = border_y[0] + 2;
    float target_y = border_y[0] + 2;
    static uint8_t blink_counter = 0;
    
    // Draw borders once at startup
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    for(int i = 0; i < 4; i++){
        for(int p = 0; p < 640; p++){
            write_buffer(lcd_buffer, p % 320, border_y[i] + (p / 320), white_color);
        }
    }
    xSemaphoreGive(spi_xSemaphore);
    while(1){
        if(Aindex > s){
            s = Aindex;
        } else if(Aindex < s - 2){
            s = Aindex + 2;
        }
        
        int relative_index = 2 - s + Aindex;
        target_y = border_y[relative_index] + 2;
        
        // Animation
        if (fabs(current_y - target_y) > 0.5) {
            current_y += (target_y - current_y) * 0.3;
        } else {
            current_y = target_y;
        }
        
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        // Clear area between borders
        // Optimized clear using memset for faster performance
        int start_index = border_y[0] * 640;
        int end_index = (border_y[3] + 52) * 640; // Clear until end of last item
        if (end_index > 153600) end_index = 153600; // Clamp to buffer size
        uint8_t bg_hi = (bg_color.r << 3) | (bg_color.g >> 3);
        uint8_t bg_lo = (bg_color.g << 5) | bg_color.b;
        
        if (bg_hi == bg_lo) {
            memset(&lcd_buffer[start_index], bg_hi, end_index - start_index);
        } else {
            // Unroll loop slightly for speed
            for (int i = start_index; i < end_index; i += 8) {
                lcd_buffer[i] = bg_hi; lcd_buffer[i+1] = bg_lo;
                lcd_buffer[i+2] = bg_hi; lcd_buffer[i+3] = bg_lo;
                lcd_buffer[i+4] = bg_hi; lcd_buffer[i+5] = bg_lo;
                lcd_buffer[i+6] = bg_hi; lcd_buffer[i+7] = bg_lo;
            }
        }

        // Draw sliding bar
        if (curr_screen == FOCUS_ALARM_SCREEN) {
             draw_rounded_rect(5, (int)current_y, 310, 48, 10, select_bg);
        }
        
        blink_counter = (blink_counter + 1) % 10;
        
        // Dim color for unselected items
        color_struct dim_color;
        dim_color.r = white_color.r >> 1;
        dim_color.g = white_color.g >> 1;
        dim_color.b = white_color.b >> 1;

        for(int i = s - 2; i <= s; i++){
            t = (alarm_array[i] >= 100000) ? (alarm_array[i] - 100000) : alarm_array[i];
            sprintf((char*)Hourbuffer, "%02d", t / 60);
            sprintf((char*)Minutebuffer, "%02d", t % 60);
            
            color_struct *text_color = (i == Aindex && curr_screen == FOCUS_ALARM_SCREEN) ? &white_color : &dim_color;
            
            show_text_transparent((uint8_t*)name_alarm[i], text_color, 20, time_y[2 - s + i], char_map30, font_pixels30);
            
            bool draw_hour = true;
            bool draw_minute = true;
            
            if (editAlarm && i == Aindex) {
                if (blink_counter < 5) {
                    if (editAlarm == EDIT_HOUR) draw_hour = false;
                    if (editAlarm == EDIT_MINUTE) draw_minute = false;
                }
            }
            
            if (draw_hour) show_text_transparent(Hourbuffer, text_color, 80, time_y[2 - s + i], char_map30, font_pixels30);
            show_text_transparent((uint8_t*)":", text_color, 113, time_y[2 - s + i] - 2, char_map30, font_pixels30);
            if (draw_minute) show_text_transparent(Minutebuffer, text_color, 123, time_y[2 - s + i], char_map30, font_pixels30);
            
            if(alarm_array[i] >= 100000){
                drawSwitch_transparent(0, 2 - s + i);
            } else {
                drawSwitch_transparent(14, 2 - s + i);
            }
        }
        xSemaphoreGive(spi_xSemaphore);
        
        vTaskDelay(20 / portTICK_PERIOD_MS);
        
        if (fabs(current_y - target_y) < 0.5 && !editAlarm && !reload) {
             while(!reload && !editAlarm && fabs(current_y - target_y) < 0.5){
                 vTaskDelay(50 / portTICK_PERIOD_MS);
                 // Check if target changed
                 int relative_index = 2 - s + Aindex;
                 float new_target_y = border_y[relative_index] + 2;
                 if(fabs(new_target_y - target_y) > 0.5) break;
             }
        }
        reload = 0;
    }
}

void redraw_alarm_static() {
    show_text((uint8_t*)"Alarm", &text_clock, &bg_color, 130, 5, char_map30, font_pixels30);
}