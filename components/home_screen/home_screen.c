#include "digit_clock.h"
#include "icon_screen1.h"
#include "st7789.h"
#include "esp_wifi.h"
#include <sys/time.h>
#include "home_screen.h"
#include "string.h"
#include "animated.h"
#include "define.h"
#include "aht30.h"
#include "mpu6050.h"

uint8_t temp_clock[8];
uint8_t buffer_date[] = {'T', '_', ' ', ' ', ' ', 'd', 'd', ' ', '-', ' ','m', 'm', ' ' ,'-' , ' ', 'y', 'y', 'y', 'y', '\0'};
const char NoInternet[] = "No Internet";


#define STACK_SIZE_DrawClock 2000
StaticTask_t xTaskBuffer_DrawClock;
StackType_t xStack_DrawClock[ STACK_SIZE_DrawClock ];

#define STACK_SIZE_WifiStatus 2000
StaticTask_t xTaskBuffer_WifiStatus;
StackType_t xStack_WifiStatus[ STACK_SIZE_WifiStatus ];

#define STACK_SIZE_DrawSun 1000
StaticTask_t xTaskBuffer_DrawSun;
StackType_t xStack_DrawSun[ STACK_SIZE_DrawSun ];

#define STACK_SIZE_DrawCloud 1000
StaticTask_t xTaskBuffer_DrawCloud;
StackType_t xStack_DrawCloud[ STACK_SIZE_DrawCloud ];

#define STACK_SIZE_DrawBird 2000
StaticTask_t xTaskBuffer_DrawBird;
StackType_t xStack_DrawBird[ STACK_SIZE_DrawBird ];

#define STACK_SIZE_PrintTH 3000
StaticTask_t xTaskBuffer_PrintTH;
StackType_t xStack_PrintTH[ STACK_SIZE_PrintTH ];

TaskHandle_t xHandle_clock;
TaskHandle_t xHandle_WifiStrength;
TaskHandle_t xHandle_DrawSun;
TaskHandle_t xHandle_DrawCloud;
TaskHandle_t xHandle_DrawBird;
TaskHandle_t xHandle_PrintTH;

void init_home(){
    curr_screen = HOME_SCREEN;
    fill_display(spi, &bg_color);
    display_date();

    xHandle_clock = xTaskCreateStatic(
                     display_clock, "Draw clock",
                     STACK_SIZE_DrawClock,  ( void * ) 2, 
                     2, xStack_DrawClock, &xTaskBuffer_DrawClock );
    configASSERT( xHandle_clock );

    xHandle_WifiStrength = xTaskCreateStatic(
                     draw_wifi_status, "Draw WifiStrength",
                     STACK_SIZE_WifiStatus, NULL, 
                     1, xStack_WifiStatus, &xTaskBuffer_WifiStatus );
    configASSERT( xHandle_WifiStrength );

    xHandle_DrawSun = xTaskCreateStatic(
                     DrawSun, "Draw DrawSun",
                     STACK_SIZE_DrawSun, NULL, 
                     2, xStack_DrawSun, &xTaskBuffer_DrawSun );
    configASSERT( xHandle_DrawSun );

    xHandle_DrawCloud = xTaskCreateStatic(
                     DrawCloud, "Draw DrawCloud",
                     STACK_SIZE_DrawCloud, NULL, 
                     2, xStack_DrawCloud, &xTaskBuffer_DrawCloud );
    configASSERT( xHandle_DrawCloud );

    xHandle_DrawBird = xTaskCreateStatic(
                     DrawBird, "Draw DrawBird",
                     STACK_SIZE_DrawBird, NULL, 
                     2, xStack_DrawBird, &xTaskBuffer_DrawBird );
    configASSERT( xHandle_DrawBird );

    xHandle_PrintTH = xTaskCreateStatic(
                     print_temperature_and_humidity, "Draw PrintTH",
                     STACK_SIZE_PrintTH, NULL, 
                     1, xStack_PrintTH, &xTaskBuffer_PrintTH );
    configASSERT( xHandle_PrintTH );
}

void stop_home(){
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    vTaskDelete( xHandle_clock );
    vTaskDelete( xHandle_WifiStrength );
    vTaskDelete( xHandle_DrawSun );
    vTaskDelete( xHandle_DrawCloud );
    vTaskDelete( xHandle_DrawBird );
    vTaskDelete( xHandle_PrintTH );
    xSemaphoreGive(spi_xSemaphore);
}

void DrawSun(){
    int i = 0;
    float alpha, oneminusalpha;
    uint8_t v;
    color_struct color;
    color = sun_color;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    uint8_t flag = (timeinfo.tm_hour >= 18 || timeinfo.tm_hour <= 6) ? 0 : 1;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        for(int y = 5; y < 55; y++){
            for(int x = 5; x < 55; x++){
                if(timeinfo.tm_hour >= 18 || timeinfo.tm_hour <= 6){
                    if(flag == 0){
                        flag = 1;
                        color = moon_color;
                    }
                    v = 255 - moonn[i * 2500 + (y - 5) * 50 + (x - 5)];
                } else {
                    if(flag == 1){
                        flag = 0;
                        color = sun_color;
                    }
                    v = 255 - sunn[i * 2500 + (y - 5) * 50 + (x - 5)];
                }
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((color.b * alpha) + (oneminusalpha * bg_color.b));
                    write_buffer(lcd_buffer, x, y, last_color);
                    last_v = v;
                } else {
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
        }
        xSemaphoreGive(spi_xSemaphore);
        i = (i + 1) % 28;
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
}

void DrawBird(){
    int16_t accel_data[3];
    int16_t gyro_data[3];
    char buffer1[20];
    char buffer2[20];
    char buffer3[20];
    int pulse_count;
    int16_t xRawMin = -14000, xRawMax = 12000, yRawMin = -10100, yRawMax = 13364, zRawMin = -14250, zRawMax = 20000;
    while(1){
        xSemaphoreTake(i2c_xSemaphore, portMAX_DELAY);
        read_mpu6050(accel_data, gyro_data);
        xSemaphoreGive(i2c_xSemaphore);
        ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
        sprintf(buffer1, "X:%" PRIi16 "   ", (int16_t)((accel_data[0] < 0) ?  -(float)accel_data[0]/xRawMin * 180 : (float)accel_data[0]/xRawMax * 180));
        sprintf(buffer2, "Y:%" PRIi16 "   ", (int16_t)((accel_data[1] < 0) ?  -(float)accel_data[1]/yRawMin * 180 : (float)accel_data[1]/yRawMax * 180));
        sprintf(buffer3, "Z:%" PRIi16 "   ", (int16_t)((accel_data[2] < 0) ?  -(float)accel_data[2]/zRawMin * 180 : (float)accel_data[2]/zRawMax * 180));
        show_text(spi, (uint8_t*)buffer1, &white_color, &bg_color, 220, 160, char_map25, font_pixels25);
        show_text(spi, (uint8_t*)buffer2, &white_color, &bg_color, 220, 185, char_map25, font_pixels25);
        show_text(spi, (uint8_t*)buffer3, &white_color, &bg_color, 220, 210, char_map25, font_pixels25);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    
    /*
    float alpha, oneminusalpha;
    uint8_t v;
    int i = 0;
    color_struct old_color;
    uint16_t old_v = 256;
    while(1){
        for(int p = 0; p < 4900; p++){
            v = 255 - birdd[i * 4900 + p]; 
            if(v != old_v){
                alpha = (float)v / 255;
                oneminusalpha = 1 - alpha;
                buffer_bird[p].r = (uint8_t)(((white_color.r >> 2) * alpha) + (oneminusalpha * (bg_color.r >> 2))) << 2;
                buffer_bird[p].g = (uint8_t)(((white_color.g >> 2) * alpha) + (oneminusalpha * (bg_color.g >> 2))) << 2;
                buffer_bird[p].b = (uint8_t)(((white_color.b >> 2) * alpha) + (oneminusalpha * (bg_color.b >> 2))) << 2;
                old_v = v;
                old_color = buffer_bird[p];
            } else {
                buffer_bird[p] = old_color;
            }
        }
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        lcd_print_data(spi, 220, 289, 160, 229, buffer_bird, 14700);
        xSemaphoreGive(spi_xSemaphore);
        i = (i + 1) % 27;
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
    */
}

void DrawCloud(){
    int Xs = 55;
    int step;
    float alpha, oneminusalpha;
    uint8_t v;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        for(int y = 25; y < 50; y++){
            for(int x = Xs; x < Xs + 27; x++){
                v = cloudd[(y - 25) * 27 + (x - Xs)];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((cloud_color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((cloud_color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((cloud_color.b * alpha) + (oneminusalpha * bg_color.b));
                    write_buffer(lcd_buffer, x, y, last_color);
                    last_v = v;
                } else {
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
        }
        xSemaphoreGive(spi_xSemaphore);
        if(Xs == 105){
            step = -1;
        } else if(Xs == 55){
            step = 1;
        }
        Xs+=step;
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
}

void display_clock(void * pvParameters) {
    time_t now;
    struct tm timeinfo;
    uint16_t Xs, Ys = 55, Xe, Ye;
    uint8_t v;
    struct clock_char font_c;
    float alpha, oneminusalpha;
    temp_clock[2] = 58;
    temp_clock[5] = 58;
    uint8_t last_clock[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    time(&now);
    localtime_r(&now, &timeinfo);
    int date = timeinfo.tm_mday;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        Xs = 15;
        time(&now);
        localtime_r(&now, &timeinfo);
        temp_clock[0] = timeinfo.tm_hour / 10 + 48;
        temp_clock[1] = timeinfo.tm_hour % 10 + 48;
        temp_clock[3] = timeinfo.tm_min / 10 + 48;
        temp_clock[4] = timeinfo.tm_min % 10 + 48;
        temp_clock[6] = timeinfo.tm_sec / 10 + 48;
        temp_clock[7] = timeinfo.tm_sec % 10 + 48;
        for(int i = 0; i < 8; i++){
            font_c = clock_map[temp_clock[i]];
            Xe = Xs + font_c.w + font_c.left;
            Ye = Ys + font_c.h + font_c.top;
            if(temp_clock[i] == last_clock[i]){
                Xs = Xs + font_c.advance;
                continue;
            }
            xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
            for(int y = Ys; y < Ye; y++){
                for(int x = Xs; x < Xe; x++){
                    write_buffer(lcd_buffer, x, y, bg_color);
                }
            }
            for(int y = Ys + font_c.top; y < Ye; y++){
                for(int x = Xs + font_c.left; x < Xe; x++){
                    v = clock_pixels[font_c.offset + (x - Xs - font_c.left) + (y - Ys - font_c.top) * font_c.w];
                    if(v != last_v){
                        alpha = (float)v / 255;
                        oneminusalpha = 1 - alpha;
                        last_color.r = (uint8_t)((clock_color.r * alpha) + (oneminusalpha * bg_color.r));
                        last_color.g = (uint8_t)((clock_color.g * alpha) + (oneminusalpha * bg_color.g));
                        last_color.b = (uint8_t)((clock_color.g * alpha) + (oneminusalpha * bg_color.b));
                        last_v = v;
                        write_buffer(lcd_buffer, x, y, last_color);
                    } else {
                        write_buffer(lcd_buffer, x, y, last_color);
                    }
                }
            }
            xSemaphoreGive(spi_xSemaphore);
            Xs = Xs + font_c.advance;
            last_clock[i] = temp_clock[i];
        }
        if(date != timeinfo.tm_mday){
            display_date();
            date = timeinfo.tm_mday;
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

void draw_wifi_status(){
    uint8_t v;
    uint16_t Xs = 285, Ys = 10, Xe, Ye;
    struct icon_char icon_c;
    float alpha, oneminusalpha;
    wifi_ap_record_t ap_info;
    esp_err_t err;
    uint16_t Xssid;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        err = esp_wifi_sta_get_ap_info(&ap_info);
        if(err == ESP_OK){
            if(ap_info.rssi < -90){
                icon_c = icon_map[1];
            } else if(ap_info.rssi < -60){
                icon_c = icon_map[2];
            } else {
                icon_c = icon_map[3];
            }
            if(strlen((char*)ap_info.ssid) > 10){
                ap_info.ssid[10] =
                ap_info.ssid[11] = '.';
                ap_info.ssid[12] = '\0';
            }
        } else {
            esp_wifi_connect();
            strcpy((char*)ap_info.ssid, NoInternet);
            icon_c = icon_map[0];
        }
        Xssid = 280 - 10;
        for(int i = 0; i < strlen((char*)ap_info.ssid); i++){
            Xssid -= char_map25[ap_info.ssid[i]].advance;
        }
        show_text(spi, (uint8_t*)"   ", &wifi_color, &bg_color, Xssid - 18, 15, char_map25, font_pixels25);
        show_text(spi, ap_info.ssid, &wifi_color, &bg_color, Xssid, 15, char_map25, font_pixels25);
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        Xe = Xs + 30;
        Ye = Ys + 30;
        for(int y = Ys; y < Ye; y++){
            for(int x = Xs; x < Xe; x++){
                write_buffer(lcd_buffer, x, y, bg_color);
            }
        }
        for(int y = Ys; y < Ye; y++){
            for(int x = Xs; x < Xe; x++){
                v = icon_pixels[icon_c.offset + (x - Xs) + (y - Ys) * icon_c.w];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((wifi_color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((wifi_color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((wifi_color.b * alpha) + (oneminusalpha * bg_color.b));
                    last_v = v;
                    write_buffer(lcd_buffer, x, y, last_color);
                } else {
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
        }
        xSemaphoreGive(spi_xSemaphore);
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

void print_temperature_and_humidity(){
    char buffer_temp[10];
    char buffer_humi[10];
    float temperature = 0, humidity = 0;
    show_text(spi, (uint8_t*)"Temperature:", &white_color, &bg_color, 20, 170, char_map25, font_pixels25);
    show_text(spi, (uint8_t*)"Humidity:", &white_color, &bg_color, 20, 200, char_map25, font_pixels25);
    while(1){
        xSemaphoreTake(i2c_xSemaphore, portMAX_DELAY);
        aht30_read(&humidity, &temperature);
        xSemaphoreGive(i2c_xSemaphore);
        sprintf(buffer_temp, "%.1f %cC   ", temperature, 176);
        sprintf(buffer_humi, "%.1f %%   ", humidity);
        show_text(spi, (uint8_t*)buffer_temp, &white_color, &bg_color, 145, 170, char_map25, font_pixels25);
        show_text(spi, (uint8_t*)buffer_humi, &white_color, &bg_color, 105, 200, char_map25, font_pixels25);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void display_date(){
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if(timeinfo.tm_wday == 0){
        buffer_date[0] = 'C';
        buffer_date[1] = 'N';
    } else {
        buffer_date[0] = 'T';
        buffer_date[1] = timeinfo.tm_wday + 1 + 48;
    }
    buffer_date[5] = timeinfo.tm_mday / 10 + 48;
    buffer_date[6] = timeinfo.tm_mday % 10 + 48;
    timeinfo.tm_mon++;
    buffer_date[10] = timeinfo.tm_mon / 10 + 48;
    buffer_date[11] = timeinfo.tm_mon % 10 + 48;
    timeinfo.tm_year+=1900;
    buffer_date[15] = timeinfo.tm_year / 1000 % 10 + 48;
    buffer_date[16] = timeinfo.tm_year / 100 % 10 + 48;
    buffer_date[17] = timeinfo.tm_year / 10 % 10 + 48;
    buffer_date[18] = timeinfo.tm_year % 10 + 48;
    show_text(spi, buffer_date, &date_color, &bg_color, 60, 120, char_map30, font_pixels30);
}