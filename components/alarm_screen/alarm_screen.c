#include "st7789.h"
#include "alarm_screen.h"
#include "animated.h"
#include "nvs_flash.h"
#include <stdint.h>
#include "define.h"
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

void DrawList();
void KeyEvent();
void update_alarmNVS(int A);
void reverse_alarm(int A);
void drawSwitch(int frame, int i, const color_struct bg_rgb);

void init_alarm(){
    curr_screen = ALARM_SCREEN;
    fill_display(spi, &bg_color);
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
    show_text(spi, (uint8_t*)"Alarm", &text_clock, &bg_color, 130, 5, char_map30, font_pixels30);
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
        if(!key1Status && !countDown && !countUp){
            vTaskDelay(30 / portTICK_PERIOD_MS);
            reload = 0;
            continue;
        }
        printf("%" PRIu8 " %" PRIu8 " %" PRIu8 "\n", key1Status, countDown, countUp);
        if(curr_screen == ALARM_SCREEN){
            if(key1Status){
                curr_screen = FOCUS_ALARM_SCREEN;
                Aindex = 0;
                reload = 1;
                editAlarm = NOT_EDIT;
            }
        } else if(curr_screen == FOCUS_ALARM_SCREEN){
            if(key1Status == CLICK_KEY){
                if(editAlarm == EDIT_HOUR){
                    editAlarm = EDIT_MINUTE;
                } else if(editAlarm == EDIT_MINUTE){
                    editAlarm = NOT_EDIT;
                    Aindex = 0;
                    curr_screen = ALARM_SCREEN;
                    update_alarmNVS(Aindex);
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                } else {
                    editAlarm = EDIT_HOUR;
                }
            } else if(key1Status == HOLD_KEY){
                reverse_alarm(Aindex);
            } else if(editAlarm){
                Astatus = (alarm_array[Aindex] >= 100000) ? 100000 : 0;
                hour = (alarm_array[Aindex] - Astatus) / 60;
                minute = (alarm_array[Aindex] - Astatus) % 60;
                if(countDown){
                    if(editAlarm == EDIT_HOUR){
                        hour = (hour + 23) % 24;
                    } else {
                        minute = (minute + 59) % 60;
                    }
                } else {
                    if(editAlarm == EDIT_HOUR){
                        hour = (hour + 1) % 24;
                    } else {
                        minute = (minute + 1) % 60;
                    }
                }
                alarm_array[Aindex] = Astatus + hour * 60 + minute;
            } else {
                if(countUp){
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
            pcnt_unit_clear_count(pcnt_unit);
            countUp  = countDown = 0;
        }
        key1Status = 0;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }
}

void reverse_alarm(int A){
    if(alarm_array[A] >= 100000){
        for(int f = 0; f < 14; f++){
            drawSwitch(f, 2 - s + Aindex, select_bg);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        alarm_array[A]-=100000;
    } else {
        for(int f = 14; f < 27; f++){
            drawSwitch(f, 2 - s + Aindex, select_bg);
            vTaskDelay(20 / portTICK_PERIOD_MS);
        }
        alarm_array[A]+=100000;
    }
}

void update_alarmNVS(int A){
    nvs_set_u32(A_nvs_handle, name_alarm[A], alarm_array[A]);
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
                write_buffer(lcd_buffer, x, y, last_color);
                last_v = v;
            } else {
                write_buffer(lcd_buffer, x, y, last_color);
            }
        }
    }
    xSemaphoreGive(spi_xSemaphore);
}

void DrawList(){
    s = 2;
    int t;
    uint8_t Hourbuffer[10];
    uint8_t Minutebuffer[10];
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    for(int i = 0; i < 4; i++){
        for(int y = border_y[i]; y < border_y[i] + 2; y++){
            for(int x = 0; x < 320; x++){
                write_buffer(lcd_buffer, x, y, white_color);
            }
        }
    }
    
    xSemaphoreGive(spi_xSemaphore);
    while(1){
        if(Aindex > s){
            s = Aindex;
        } else if(Aindex < s - 2){
            s = Aindex + 2;
        }
        for(int i = s - 2; i <= s; i++){
            t = (alarm_array[i] >= 100000) ? (alarm_array[i] - 100000) : alarm_array[i];
            sprintf((char*)Hourbuffer, "%02d", t / 60);
            sprintf((char*)Minutebuffer, "%02d", t % 60);
            if(i == Aindex && curr_screen == FOCUS_ALARM_SCREEN){
                xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
                for(int y = border_y[2 - s + i] + 2; y < border_y[2 - s + i] + 52; y++){
                    for(int x = 0; x < 320; x++){
                        write_buffer(lcd_buffer, x, y, select_bg);
                    }
                }
                xSemaphoreGive(spi_xSemaphore);
                if(alarm_array[i] >= 100000){
                    drawSwitch(0, 2 - s + i, select_bg);
                } else {
                    drawSwitch(14, 2 - s + i, select_bg);
                }
                show_text(spi, (uint8_t*)name_alarm[i], &white_color, &select_bg, 20, time_y[2 - s + i], char_map30, font_pixels30);
                show_text(spi, Hourbuffer, &white_color, &select_bg, 80, time_y[2 - s + i], char_map30, font_pixels30);
                show_text(spi, (uint8_t*)":", &white_color, &select_bg, 110, time_y[2 - s + i] - 2, char_map30, font_pixels30);
                show_text(spi, Minutebuffer, &white_color, &select_bg, 120, time_y[2 - s + i], char_map30, font_pixels30);
            } else {
                xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
                for(int y = border_y[2 - s + i] + 2; y < border_y[2 - s + i] + 52; y++){
                    for(int x = 0; x < 320; x++){
                        write_buffer(lcd_buffer, x, y, bg_color);
                    }
                }
                xSemaphoreGive(spi_xSemaphore);
                if(alarm_array[i] >= 100000){
                    drawSwitch(0, 2 - s + i, bg_color);
                } else {
                    drawSwitch(14, 2 - s + i, bg_color);
                }
                show_text(spi, (uint8_t*)name_alarm[i], &white_color, &bg_color, 20, time_y[2 - s + i], char_map30, font_pixels30);
                show_text(spi, Hourbuffer, &white_color, &bg_color, 80, time_y[2 - s + i], char_map30, font_pixels30);
                show_text(spi, (uint8_t*)":", &white_color, &bg_color, 110, time_y[2 - s + i] - 2, char_map30, font_pixels30);
                show_text(spi, Minutebuffer, &white_color, &bg_color, 120, time_y[2 - s + i], char_map30, font_pixels30);
            }
        }
        t = (alarm_array[Aindex] >= 100000) ? (alarm_array[Aindex] - 100000) : alarm_array[Aindex];
        sprintf((char*)Hourbuffer, "%02d", t / 60);
        sprintf((char*)Minutebuffer, "%02d", t % 60);
        uint8_t c = 0;
        while(editAlarm){
            if(reload){
                t = (alarm_array[Aindex] >= 100000) ? (alarm_array[Aindex] - 100000) : alarm_array[Aindex];
                sprintf((char*)Hourbuffer, "%02d", t / 60);
                sprintf((char*)Minutebuffer, "%02d", t % 60);
                if(editAlarm == EDIT_HOUR){
                    show_text(spi, Hourbuffer, &white_color, &select_bg, 80, time_y[2 - s + Aindex], char_map30, font_pixels30);
                } else {
                    show_text(spi, Minutebuffer, &white_color, &select_bg, 120, time_y[2 - s + Aindex], char_map30, font_pixels30);
                }
            } else {
                if(c == 0){
                    if(editAlarm == EDIT_HOUR){
                        show_text(spi, (uint8_t*)"     ", &white_color, &select_bg, 80, time_y[2 - s + Aindex], char_map30, font_pixels30);
                    } else {
                        show_text(spi, (uint8_t*)"     ", &white_color, &select_bg, 120, time_y[2 - s + Aindex], char_map30, font_pixels30);
                    }
                } else if(c == 5){
                    if(editAlarm == EDIT_HOUR){
                        show_text(spi, Hourbuffer, &white_color, &select_bg, 80, time_y[2 - s + Aindex], char_map30, font_pixels30);
                    } else {
                        show_text(spi, Minutebuffer, &white_color, &select_bg, 120, time_y[2 - s + Aindex], char_map30, font_pixels30);
                    }
                }
                c = (c + 1) % 11;
            }
            reload = 0;
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
        while(!reload){
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}