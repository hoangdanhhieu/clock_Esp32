#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "soc/gpio_sig_map.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_sleep.h"
#include "esp_sntp.h"
#include "esp_system.h"
#include "esp_tls.h"
#include "esp_tls_errors.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lwip/err.h"
#include "lwip/ip_addr.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "ili9341.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "home_screen.h"
#include "alarm_screen.h"
#include "settings_screen.h"
#include "animated.h"
#include "digit_clock.h"
#include "define.h"
#include "ble_gatts_server.c"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "driver/usb_serial_jtag.h"
#include "aht30.h"
#include "mpu6050.h"
#include "temt6000.h"
#include "vl53l0x_init.h"

#ifdef ENABLE_COLOR_DESIGNER
#include "color.h"
#endif


spi_device_handle_t spi;
i2c_master_bus_handle_t i2c_master_bus_handle;
#define ESP_WIFI_SSID ""
#define ESP_WIFI_PASS ""
#define ESP_MAXIMUM_RETRY 4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK

#define ESP_WIFI_AP_SSID "ESP32_Clock_Config"
#define ESP_WIFI_AP_PASS "12345678"
#define ESP_WIFI_AP_CHANNEL 1
#define ESP_WIFI_AP_MAX_CONN 4


#define MAX_HTTP_OUTPUT_BUFFER 1000


static const char *TAG_STATION = "wifi station";
static const char *TAG_AP = "wifi softAP";
static const char *TAG_WEB = "webserver";
static const char *TAG_SNTP = "SNTP";
static const char *TAG_HTTP = "http";
static const char *TAG_timenow = "time now";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static bool ap_mode_active = false;
static bool sta_connected = false;
static httpd_handle_t webserver = NULL;

// WiFi credentials storage in NVS
char saved_ssid[32] = ESP_WIFI_SSID;
char saved_password[64] = ESP_WIFI_PASS;

time_t update_time = 10000000;
char http_buffer[1000];
DMA_ATTR uint8_t lcd_buffer[153600];
SemaphoreHandle_t spi_xSemaphore;
SemaphoreHandle_t i2c_xSemaphore;
TaskHandle_t xHandle = NULL;

VL53L0X_Dev_t MyDevice;
VL53L0X_Dev_t *pMyDevice;
VL53L0X_Version_t Version;
VL53L0X_Version_t *pVersion;
VL53L0X_DeviceInfo_t DeviceInfo;

uint8_t key1Status,key2Status, key3Status;
#define holdTimeUs 300000

uint32_t alarm_array[10];
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

uint8_t curr_screen = HOME_SCREEN;
uint8_t is_night_mode = 0;

void wifi_init_sta(void);
void wifi_init_apsta(void);
void wifi_reconnect_task(void *pvParameters);
void start_webserver(void);
void stop_webserver(void);
void save_wifi_credentials(const char* ssid, const char* password);
void load_wifi_credentials(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);
void buzzer_pwm_init();
void time_sync_notification_cb(struct timeval *tv);
void gpio_init();
void http_init();
void time_init();
void sntp_init_t();
void scan_key();
void init_alarm_nvs();
void alarm_loop();
void theme_loop();
void stop_screen();
void alarm_noti_screen();
void refresh_screen();
void slide_transition(uint8_t direction);
void vl53l0x_device_init();
void init_i2c_devices();
void init_i2c_bus(); 
void remove_i2c_devices();
void remove_i2c_bus();


#ifdef ENABLE_COLOR_DESIGNER
void color_designer_task(void *pvParameters) {
    // Configure USB-Serial-JTAG driver
    usb_serial_jtag_driver_config_t usb_serial_config = {
        .rx_buffer_size = 256,
        .tx_buffer_size = 256,
    };
    
    // Install USB-Serial-JTAG driver
    usb_serial_jtag_driver_install(&usb_serial_config);
    
    char cmd_buffer[128];
    int cmd_pos = 0;
    
    printf("\n=== Color Designer Ready ===\n");
    printf("Commands: help, list, get <name>, set <name> <r> <g> <b>\n");
    printf("============================\n");
    printf("> ");
    fflush(stdout);
    
    while(1) {
        uint8_t data;
        int len = usb_serial_jtag_read_bytes(&data, 1, 20 / portTICK_PERIOD_MS);
        
        if(len > 0) {
            if(data == '\n' || data == '\r') {
                if(cmd_pos > 0) {
                    printf("\n");
                    cmd_buffer[cmd_pos] = '\0';
                    color_designer_process_command(cmd_buffer);
                    cmd_pos = 0;
                    printf("> ");
                    fflush(stdout);
                }
            } else if(data == 8 || data == 127) {  // Backspace
                if(cmd_pos > 0) {
                    cmd_pos--;
                    printf("\b \b");  // Erase character on screen
                    fflush(stdout);
                }
            } else if(cmd_pos < sizeof(cmd_buffer) - 1 && data >= 32) {
                cmd_buffer[cmd_pos++] = data;
                printf("%c", data);  // Echo character
                fflush(stdout);
            }
        }
        
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
#endif

void set_manual_time(int year, int month, int day, int hour, int minute, int second) {
    struct tm tm_config;
    
    // Cấu hình struct tm (Lưu ý: tháng bắt đầu từ 0, năm tính từ 1900)
    tm_config.tm_year = year - 1900; 
    tm_config.tm_mon  = month - 1;   
    tm_config.tm_mday = day;
    tm_config.tm_hour = hour;
    tm_config.tm_min  = minute;
    tm_config.tm_sec  = second;
    tm_config.tm_isdst = 0; // Không dùng giờ mùa hè

    time_t t = mktime(&tm_config); // Chuyển đổi sang timestamp (giây tính từ 1970)

    struct timeval now = { .tv_sec = t };
    
    // Gọi lệnh hệ thống để set thời gian
    settimeofday(&now, NULL);
    
    // Cài đặt múi giờ cho Việt Nam (UTC+7)
    // Cú pháp POSIX: "tên_múi_giờ <offset>". Với VN là GMT-7 (đảo dấu)
    setenv("TZ", "CST-7", 1); 
    tzset();

    ESP_LOGI("TIME", "Đã đặt thời gian hệ thống!");
}

void app_main(void) {
    spi_xSemaphore = xSemaphoreCreateMutex();
    i2c_xSemaphore = xSemaphoreCreateMutex();

    gpio_init();
    spi2_lcd_init(&spi);

    xTaskCreate( refresh_screen, "refresh_screen", 2000, NULL, 2, &xHandle );
    configASSERT( xHandle );

    buzzer_pwm_init();
    init_i2c_bus();
    init_i2c_devices();
    init_adc_temt6000();
    init_alarm_nvs();

    show_text((uint8_t*)"Connecting...", &white_color, &bg_color, 100, 100, char_map30, font_pixels30);
    wifi_ap_record_t ap_info;
    ESP_LOGI(TAG_STATION, "ESP_WIFI_MODE_STA");
    
    // Load saved credentials first
    load_wifi_credentials();
    wifi_init_sta();
    
    // Wait up to 10 seconds for WiFi connection
    int wait_count = 0;
    while(wait_count < 20) {  // 20 * 500ms = 10 seconds
        esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
        if(err == ESP_OK) {
            sta_connected = true;
            break;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
        wait_count++;
    }

    // If WiFi connection fails, start AP mode
    if(!sta_connected){
        ESP_LOGI(TAG_STATION, "Cannot connect to WiFi, starting AP mode");
        fill_display(&bg_color);
        show_text((uint8_t*)"WiFi Config Mode", &white_color, &bg_color, 65, 60, char_map30, font_pixels30);
        show_text((uint8_t*)"1. Connect to WiFi:", &white_color, &bg_color, 20, 100, char_map25, font_pixels25);
        show_text((uint8_t*)"   ESP32_Clock_Config", &white_color, &bg_color, 20, 125, char_map25, font_pixels25);
        show_text((uint8_t*)"   Password: 12345678", &white_color, &bg_color, 20, 145, char_map25, font_pixels25);
        show_text((uint8_t*)"2. Open browser:", &white_color, &bg_color, 20, 175, char_map25, font_pixels25);
        show_text((uint8_t*)"   http://192.168.4.1", &white_color, &bg_color, 20, 195, char_map25, font_pixels25);
        
        wifi_init_apsta();
        
        // Start reconnection task
        xTaskCreate(wifi_reconnect_task, "wifi_reconnect", 3072, NULL, 1, NULL);
        
        // Wait for connection in AP mode
        while(!sta_connected){
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        
        // Connected! Update display
        fill_display(&bg_color);
        show_text((uint8_t*)"WiFi Connected!", &white_color, &bg_color, 80, 100, char_map30, font_pixels30);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    } else {
        ESP_LOGI(TAG_STATION, "Connected to WiFi successfully");
    }

    fill_display(&bg_color);
    show_text((uint8_t*)"Update time...", &white_color, &bg_color, 100, 100, char_map30, font_pixels30);
    time_init();
    //set_manual_time(2024, 6, 1, 11, 40, 0); // Set time to June 1, 2024, 06:00:00
    init_home();
    //uint16_t ttt;
    // while(1){
    //     vl53l0x_GetRanging_now(pMyDevice, &ttt);
    //     printf("%d       %" PRIu16 "\n", read_temt6000_value(), ttt);
    // }
    //vTaskDelay(7000000 / portTICK_PERIOD_MS);
    xTaskCreate(scan_key, "keyScan",2000, NULL, 2, &xHandle);
    configASSERT(xHandle);
    xTaskCreate(alarm_loop, "AlarmLoop", 5000, NULL, 2, &xHandle);
    configASSERT(xHandle);
    xTaskCreate(theme_loop, "ThemeLoop", 2000, NULL, 2, &xHandle);
    configASSERT(xHandle);
    
#ifdef ENABLE_COLOR_DESIGNER
    xTaskCreate(color_designer_task, "ColorDesigner", 2048, NULL, 1, &xHandle);
    configASSERT(xHandle);
    ESP_LOGI("ColorDesigner", "Color designer enabled. Use USB serial to adjust colors.");
#endif
    
    while (1) {
        if(curr_screen == HOME_SCREEN || curr_screen == ALARM_SCREEN || curr_screen == SETTINGS_SCREEN){
            if (key1Status || key2Status || key3Status) {
                if(key1Status && curr_screen == HOME_SCREEN){
                    slide_transition(0);  // Slide left first
                    stop_home();
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    fill_display(&bg_color);
                    init_alarm();
                } else if(key2Status && curr_screen == ALARM_SCREEN){
                    slide_transition(1);  // Slide right first
                    stop_alarm();
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    fill_display(&bg_color);
                    init_home();
                } else if(key2Status && curr_screen == HOME_SCREEN){
                    slide_transition(1); // Slide right to Settings
                    stop_home();
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    fill_display(&bg_color);
                    init_settings();
                } else if(key1Status && curr_screen == SETTINGS_SCREEN){
                    slide_transition(0); // Slide left to Home
                    stop_settings();
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    fill_display(&bg_color);
                    init_home();
                }
                key1Status = key2Status = NOTHING;
            }
        }
        vTaskDelay(70 / portTICK_PERIOD_MS);
    }
}


void init_i2c_devices(){
    init_mpu6050(i2c_master_bus_handle);
    init_aht30(i2c_master_bus_handle);
    //vl53l0x_device_init();
}

void remove_i2c_devices(){
    remove_mpu6050();
    remove_aht30();
}

void remove_i2c_bus(){
    ESP_ERROR_CHECK(i2c_del_master_bus(i2c_master_bus_handle));
}

void init_i2c_bus(){
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = PIN_NUM_SCL,
        .sda_io_num = PIN_NUM_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };  
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_master_bus_handle));
}

void i2c_reset(){
    remove_i2c_devices();
    remove_i2c_bus();
    //set pull up scl and sda
    gpio_reset_pin(PIN_NUM_SCL);
    gpio_reset_pin(PIN_NUM_SDA);
    gpio_set_pull_mode(PIN_NUM_SCL, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(PIN_NUM_SDA, GPIO_PULLUP_ONLY);
    //set high
    gpio_set_direction(PIN_NUM_SCL, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_SDA, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_NUM_SCL, 1);
    gpio_set_level(PIN_NUM_SDA, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    init_i2c_bus();
    init_i2c_devices();
}

void i2c_probe(){
    i2c_master_bus_config_t i2c_mst_config_1 = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = PIN_NUM_SCL,
        .sda_io_num = PIN_NUM_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    esp_err_t e; 
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config_1, &bus_handle));
    for(uint8_t i = 0; i < 128; i++){
        e = i2c_master_probe(bus_handle, i, 100);
        if(e == ESP_OK){
            printf("Address: %" PRIu8 "\n", i);
        }
    }
    i2c_del_master_bus(bus_handle);
}

void update_screen_background(color_struct old_bg, color_struct new_bg) {
    uint8_t old_hi = (old_bg.r << 3) | (old_bg.g >> 3);
    uint8_t old_lo = (old_bg.g << 5) | old_bg.b;
    uint8_t new_hi = (new_bg.r << 3) | (new_bg.g >> 3);
    uint8_t new_lo = (new_bg.g << 5) | new_bg.b;

    if (old_hi == new_hi && old_lo == new_lo) return;

    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    // Process 2 bytes at a time (1 pixel)
    for (int i = 0; i < 153600; i += 2) {
        if (lcd_buffer[i] == old_hi && lcd_buffer[i+1] == old_lo) {
            lcd_buffer[i] = new_hi;
            lcd_buffer[i+1] = new_lo;
        }
    }
    xSemaphoreGive(spi_xSemaphore);
}

void theme_loop(){
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    uint8_t is_night = 255;
    
    // Capture the current background color (which is on the screen)
    color_struct current_bg = bg_color;
    
    // Calculate the new background color based on time
    getSkyColorRGB565((float)timeinfo.tm_hour + (float)timeinfo.tm_min / 60.0f);
    
    // If the calculated color is different from the screen color, update immediately
    if(bg_color.r != current_bg.r || bg_color.g != current_bg.g || bg_color.b != current_bg.b){
        update_screen_background(current_bg, bg_color);
        if(curr_screen == HOME_SCREEN) {
            redraw_home_static();
        } else {
            redraw_alarm_static();
        }
    }
    current_bg = bg_color;

    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);
        
        // Calculate new color with seconds for smoothness
        getSkyColorRGB565((float)timeinfo.tm_hour + (float)timeinfo.tm_min / 60.0f + (float)timeinfo.tm_sec / 3600.0f);
        
        uint8_t new_is_night = (timeinfo.tm_hour >= 18 || timeinfo.tm_hour <= 6);
        if(new_is_night != is_night){
            is_night = new_is_night;
            is_night_mode = is_night;
        }

        if(bg_color.r != current_bg.r || bg_color.g != current_bg.g || bg_color.b != current_bg.b){
            if(curr_screen == HOME_SCREEN || curr_screen == ALARM_SCREEN || curr_screen == FOCUS_ALARM_SCREEN || curr_screen == SETTINGS_SCREEN || curr_screen == FOCUS_SETTINGS_SCREEN){
                // Update background pixels in buffer without restarting screen
                update_screen_background(current_bg, bg_color);
                
                if(curr_screen == HOME_SCREEN) {
                    redraw_home_static();
                } else if (curr_screen == ALARM_SCREEN || curr_screen == FOCUS_ALARM_SCREEN) {
                    redraw_alarm_static();
                } else if (curr_screen == SETTINGS_SCREEN || curr_screen == FOCUS_SETTINGS_SCREEN) {
                    redraw_settings_static();
                }
            }
            current_bg = bg_color;
        }
        
        // Update more frequently (every 1 second)
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}


void stop_screen(){
    if(curr_screen == HOME_SCREEN){
        stop_home();
    } else if(curr_screen == ALARM_SCREEN || curr_screen == FOCUS_ALARM_SCREEN){
        stop_alarm();
    } else if(curr_screen == SETTINGS_SCREEN || curr_screen == FOCUS_SETTINGS_SCREEN){
        stop_settings();
    }
}

void vl53l0x_device_init(){
    pMyDevice  = &MyDevice;
	pMyDevice->I2cDevAddr      = 0x29;
	pMyDevice->comms_type      =  1;
	pMyDevice->comms_speed_khz =  100;
	pMyDevice->I2cHandle = NULL;
	pVersion = &Version;
    vTaskDelay(10 / portTICK_PERIOD_MS);
	vl53l0x_init(i2c_master_bus_handle, pMyDevice, pVersion, &DeviceInfo, 0x29);
	vTaskDelay(10 / portTICK_PERIOD_MS);
}

void alarm_loop(){
    int hour, minutes;
    time_t now;
    struct tm timeinfo;
    int64_t count;
    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);
        for(int i = 0; i < 10; i++){
            if(alarm_array[i] >= 100000){
                hour = (alarm_array[i] - 100000) / 60;
                minutes = (alarm_array[i] - 100000) % 60;
                if(hour != timeinfo.tm_hour || minutes != timeinfo.tm_min){ continue; }
                stop_screen();
                curr_screen = ALARM_RING;
                alarm_noti_screen(hour, minutes);
                count = 0;
                while(!key1Status && !key2Status && !key3Status && count < 600){ //
                    uint32_t duty = (get_alarm_volume() * 4000) / 100;
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, duty);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
                    vTaskDelay(250 / portTICK_PERIOD_MS);
                    count++;
                }
                key1Status = key2Status = key3Status = NOTHING;
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
                fill_display(&bg_color);
                init_home();
                // Wait until minute changes OR alarm is disabled
                while(hour == timeinfo.tm_hour && minutes == timeinfo.tm_min && alarm_array[i] >= 100000){
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

// Fast fade transition effect - optimized for microcontrollers
// direction: 0 = fade out/in left, 1 = fade out/in right (parameter kept for API compatibility)
void slide_transition(uint8_t direction) {
    (void)direction;  // Unused, fade is same for both directions
    
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    
    // Quick 3-step fade to background color
    const uint8_t bg_byte1 = ((bg_color.r << 3) | (bg_color.g >> 3));
    const uint8_t bg_byte2 = ((bg_color.g << 5) | bg_color.b);
    
    for(int step = 0; step < 3; step++) {
        // Blend current pixels toward background
        for(int i = 0; i < 153600; i += 6) {  // Process every 3rd pixel for speed
            // Simple average with background (fast fade)
            lcd_buffer[i] = (lcd_buffer[i] + bg_byte1) >> 1;
            lcd_buffer[i + 1] = (lcd_buffer[i + 1] + bg_byte2) >> 1;
        }
        
        // Update display
        const uint16_t chunk_size = 25600;
        const uint16_t y_ranges[6][2] = {{0, 39}, {40, 79}, {80, 119}, {120, 159}, {160, 199}, {200, 239}};
        for(int i = 0; i < 6; i++){
            lcd_send_buffer(spi, 0, 319, y_ranges[i][0], y_ranges[i][1], 
                          (uint8_t*)lcd_buffer + (i * chunk_size), chunk_size);
        }
        
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
    
    xSemaphoreGive(spi_xSemaphore);
}

void refresh_screen(){
    const uint16_t chunk_size = 25600;
    const uint16_t y_ranges[6][2] = {{0, 39}, {40, 79}, {80, 119}, {120, 159}, {160, 199}, {200, 239}};
    while(1){
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        for(int i = 0; i < 6; i++){
            lcd_send_buffer(spi, 0, 319, y_ranges[i][0], y_ranges[i][1], 
                          (uint8_t*)lcd_buffer + (i * chunk_size), chunk_size);
        }
        xSemaphoreGive(spi_xSemaphore);
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void alarm_noti_screen(int hour, int minutes){
    float alpha, oneminusalpha;
    uint8_t v;
    uint8_t temp_clock[5];
    color_struct last_color;
    uint16_t last_v = 256;
    uint16_t Xs = 60, Ys = 110;
    uint16_t Xe, Ye;
    struct clock_char font_c;
    temp_clock[0] = hour / 10 + 48;
    temp_clock[1] = hour % 10 + 48;
    temp_clock[2] = 58;
    temp_clock[3] = minutes / 10 + 48;
    temp_clock[4] = minutes % 10 + 48;
    fill_display(&bg_color);
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    // Draw alarm ring icon
    for(int p = 0; p < 10000; p++){
        v = ringg[p];
        if(v != last_v){
            alpha = (float)v / 255;
            oneminusalpha = 1 - alpha;
            last_color.r = (uint8_t)((alarm_ring_color.r * alpha) + (oneminusalpha * bg_color.r));
            last_color.g = (uint8_t)((alarm_ring_color.g * alpha) + (oneminusalpha * bg_color.g));
            last_color.b = (uint8_t)((alarm_ring_color.b * alpha) + (oneminusalpha * bg_color.b));
            last_v = v;
        }
        write_buffer(lcd_buffer, 110 + (p % 100), 10 + (p / 100), last_color);
    }
    // Draw time digits
    for(int i = 0; i < 5; i++){
        last_v = 256;  // Reset for each digit
        font_c = clock_map[temp_clock[i]];
        Xe = Xs + font_c.w + font_c.left;
        Ye = Ys + font_c.h + font_c.top;
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
                    last_color.r = (uint8_t)((white_color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((white_color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((white_color.b * alpha) + (oneminusalpha * bg_color.b));
                    last_v = v;
                }
                write_buffer(lcd_buffer, x, y, last_color);
            }
        }
        Xs = Xs + font_c.advance;
    }
    xSemaphoreGive(spi_xSemaphore);
    
}

void init_alarm_nvs(){
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
        err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    nvs_handle_t A_nvs_handle;
    err = nvs_open("Alarm", NVS_READWRITE, &A_nvs_handle);
    nvs_type_t isCreate;
    nvs_find_key(A_nvs_handle, "A1", &isCreate);
    if(isCreate == ESP_ERR_NVS_NOT_FOUND){
        nvs_erase_all(A_nvs_handle);
        nvs_commit(A_nvs_handle);
        for(int i = 0; i < 10; i++){
            nvs_set_u32(A_nvs_handle, name_alarm[i], 0);
        }
    }
    for(int i = 0; i < 10; i++){
        nvs_get_u32(A_nvs_handle, name_alarm[i], &alarm_array[i]);
    }
    nvs_close(A_nvs_handle);
}


void scan_key(){
    int key1, key2, key3;
    int key1_last = 1, key2_last = 1, key3_last = 1;  // Assume released (pull-up = HIGH)
    bool flag1 = false, flag2 = false, flag3 = false;
    key1Status = key2Status = key3Status = 0;
    int64_t last_time1 = 0, last_time2 = 0, last_time3 = 0;
    
    while(1){
        // Read current button states
        key1 = gpio_get_level(PIN_NUM_BTN1);
        key2 = gpio_get_level(PIN_NUM_BTN2);
        key3 = gpio_get_level(PIN_NUM_BTN3);
        
        // Debounce - only process if state changed
        if(key1 != key1_last) {
            vTaskDelay(10 / portTICK_PERIOD_MS);  // Debounce delay
            key1 = gpio_get_level(PIN_NUM_BTN1);  // Re-read
        }
        if(key2 != key2_last) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            key2 = gpio_get_level(PIN_NUM_BTN2);
        }
        if(key3 != key3_last) {
            vTaskDelay(10 / portTICK_PERIOD_MS);
            key3 = gpio_get_level(PIN_NUM_BTN3);
        }
        
        key1_last = key1;
        key2_last = key2;
        key3_last = key3;
        // Button 1 - Active LOW (pressed = 0, released = 1)
        if(!key1 && !flag1) {  // Button just pressed
            last_time1 = esp_timer_get_time();
            flag1 = true;
        } else if(!key1 && flag1) {  // Button still held
            if(esp_timer_get_time() - last_time1 > holdTimeUs && key1Status != HOLD_KEY) {
                key1Status = HOLD_KEY;  // Set hold status once
            }
        } else if(key1 && flag1) {  // Button released
            if(key1Status != HOLD_KEY) {
                key1Status = CLICK_KEY;  // Short click
            }
            flag1 = false;
        }
        
        // Button 2
        if(!key2 && !flag2) {
            last_time2 = esp_timer_get_time();
            flag2 = true;
        } else if(!key2 && flag2) {
            if(esp_timer_get_time() - last_time2 > holdTimeUs && key2Status != HOLD_KEY) {
                key2Status = HOLD_KEY;
            }
        } else if(key2 && flag2) {
            if(key2Status != HOLD_KEY) {
                key2Status = CLICK_KEY;
            }
            flag2 = false;
        }
        
        // Button 3
        if(!key3 && !flag3) {
            last_time3 = esp_timer_get_time();
            flag3 = true;
        } else if(!key3 && flag3) {
            if(esp_timer_get_time() - last_time3 > holdTimeUs && key3Status != HOLD_KEY) {
                key3Status = HOLD_KEY;
            }
        } else if(key3 && flag3) {
            if(key3Status != HOLD_KEY) {
                key3Status = CLICK_KEY;
            }
            flag3 = false;
        }
        
        vTaskDelay(20 / portTICK_PERIOD_MS);  // Faster polling
    }
    
}

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static char *output_buffer; // Buffer to store response of http request from
                                // event handler
    static int output_len;      // Stores number of bytes read
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_CONNECTED");
        break;
    case HTTP_EVENT_HEADER_SENT:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_HEADER_SENT");
        break;
    case HTTP_EVENT_ON_HEADER:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_HEADER, key=%s, value=%s",
                 evt->header_key, evt->header_value);
        break;
    case HTTP_EVENT_ON_DATA:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
        // Clean the buffer in case of a new request
        if (output_len == 0 && evt->user_data) {
            // we are just starting to copy the output data into the use
            memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
        }
        /*
         *  Check for chunked encoding is added as the URL for chunked encoding used
         * in this example returns binary data. However, event handler can also be
         * used in case chunked encoding is used.
         */
        if (!esp_http_client_is_chunked_response(evt->client)) {
            // If user_data buffer is configured, copy the response into the buffer
            int copy_len = 0;
            if (evt->user_data) {
                // The last byte in evt->user_data is kept for the NULL character in
                // case of out-of-bound access.
                copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                if (copy_len) {
                    memcpy(evt->user_data + output_len, evt->data, copy_len);
                }
            } else {
                int content_len = esp_http_client_get_content_length(evt->client);
                if (output_buffer == NULL) {
                    // We initialize output_buffer with 0 because it is used by strlen()
                    // and similar functions therefore should be null terminated.
                    output_buffer = (char *)calloc(content_len + 1, sizeof(char));
                    output_len = 0;
                    if (output_buffer == NULL) {
                        ESP_LOGE(TAG_HTTP, "Failed to allocate memory for output buffer");
                        return ESP_FAIL;
                    }
                }
                copy_len = MIN(evt->data_len, (content_len - output_len));
                if (copy_len) {
                    memcpy(output_buffer + output_len, evt->data, copy_len);
                }
            }
            output_len += copy_len;
        }

        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_ON_FINISH");
        if (output_buffer != NULL) {
            // Response is accumulated in output_buffer. Uncomment the below line to
            // print the accumulated response ESP_LOG_BUFFER_HEX(TAG, output_buffer,
            // output_len);
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG_HTTP, "HTTP_EVENT_DISCONNECTED");
        int mbedtls_err = 0;
        esp_err_t err = esp_tls_get_and_clear_last_error(
            (esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
        if (err != 0) {
            ESP_LOGI(TAG_HTTP, "Last esp error code: 0x%x", err);
            ESP_LOGI(TAG_HTTP, "Last mbedtls failure: 0x%x", mbedtls_err);
        }
        if (output_buffer != NULL) {
            free(output_buffer);
            output_buffer = NULL;
        }
        output_len = 0;
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGD(TAG_HTTP, "HTTP_EVENT_REDIRECT");
        esp_http_client_set_header(evt->client, "From", "user@example.com");
        esp_http_client_set_header(evt->client, "Accept", "text/html");
        esp_http_client_set_redirection(evt->client);
        break;
    }
    return ESP_OK;
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        sta_connected = false;
        if (s_retry_num < ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG_STATION, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG_STATION, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STATION, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        sta_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG_AP, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        sta_connected = false;
        ESP_LOGI(TAG_STATION, "disconnected from AP");
    } else if (event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG_STATION, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        sta_connected = true;
    }
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
                .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                .sae_pwe_h2e = WPA3_SAE_PK_MODE_AUTOMATIC},
    };
    
    // Use saved credentials
    strcpy((char *)wifi_config.sta.ssid, saved_ssid);
    strcpy((char *)wifi_config.sta.password, saved_password);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_STATION, "wifi_init_sta finished. Trying to connect to: %s", saved_ssid);

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
     * bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, 15000 / portTICK_PERIOD_MS);  // 15 second timeout

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
     * can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_STATION, "connected to ap SSID:%s", saved_ssid);
        sta_connected = true;
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_STATION, "Failed to connect to SSID:%s", saved_ssid);
        sta_connected = false;
    } else {
        ESP_LOGE(TAG_STATION, "Connection timeout");
        sta_connected = false;
    }
}

void buzzer_pwm_init() {
    ledc_timer_config_t pwm_tmr_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .timer_num = LEDC_TIMER_2,
        .freq_hz = 2500,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ledc_channel_config_t pwm_chn_cfg = {
        .gpio_num = PIN_NUM_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_2,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_2,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&pwm_tmr_cfg));
    ESP_ERROR_CHECK(ledc_channel_config(&pwm_chn_cfg));
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_2);
}

void gpio_init() {
    gpio_config_t io_conf_lcd = {};
    io_conf_lcd.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RS));
    io_conf_lcd.mode = GPIO_MODE_OUTPUT;
    io_conf_lcd.pull_up_en = true;
    gpio_config(&io_conf_lcd);

    gpio_config_t io_conf_btn = {};
    io_conf_btn.pin_bit_mask = (1ULL << PIN_NUM_BTN1) | (1ULL << PIN_NUM_BTN2) | (1ULL << PIN_NUM_BTN3);
    io_conf_btn.mode = GPIO_MODE_INPUT;
    io_conf_btn.pull_up_en = GPIO_PULLUP_ENABLE;  // Enable pull-ups for buttons
    io_conf_btn.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf_btn);
}

void http_init() {
    esp_http_client_config_t http_cfg = {
        .url = "http://worldtimeapi.org/api/timezone/Asia/Ho_Chi_Minh",
        .buffer_size = 1000,
        .user_data = http_buffer,
        .event_handler = _http_event_handler,
    };
    esp_http_client_handle_t http_client = esp_http_client_init(&http_cfg);
    esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    esp_http_client_perform(http_client);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG_HTTP, "%s\n", http_buffer);
}


void SNTP_callback(struct timeval *tv){
    update_time = tv->tv_sec;
}
void time_init() {
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // Set timezone first
    setenv("TZ", "<+07>-7", 1);
    tzset();

    time_t now;
    struct tm timeinfo;
    sntp_init_t();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    time(&now);
    localtime_r(&now, &timeinfo);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG_timenow, "The current date/time is: %s", strftime_buf);
}

void sntp_init_t() {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("time.google.com");
    esp_netif_sntp_init(&config);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    sntp_set_time_sync_notification_cb(SNTP_callback);
    int retry = 0;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry++;
        ESP_LOGI(TAG_SNTP, "Waiting for system time to be set... (%d)", retry);
        if(retry > 10) break;
    }
}

// HTML page for WiFi configuration
static const char config_html[] = 
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>ESP32 Clock WiFi Config</title><style>"
"body{font-family:Arial;margin:20px;background:#f0f0f0}"
".container{max-width:500px;margin:0 auto;background:white;padding:20px;border-radius:10px;box-shadow:0 2px 10px rgba(0,0,0,0.1)}"
"h1{color:#333;text-align:center}"
".scan-btn{background:#4CAF50;color:white;padding:10px 20px;border:none;border-radius:5px;cursor:pointer;width:100%;font-size:16px;margin-bottom:20px}"
".scan-btn:hover{background:#45a049}"
".network{background:#f9f9f9;padding:12px;margin:5px 0;border-radius:5px;cursor:pointer;border:2px solid transparent}"
".network:hover{background:#e8f5e9;border-color:#4CAF50}"
".network strong{display:block;color:#333}"
".network small{color:#666}"
"input[type=password]{width:100%;padding:12px;margin:10px 0;border:1px solid #ddd;border-radius:5px;box-sizing:border-box;font-size:16px}"
".submit-btn{background:#2196F3;color:white;padding:12px;border:none;border-radius:5px;cursor:pointer;width:100%;font-size:16px;margin-top:10px}"
".submit-btn:hover{background:#0b7dda}"
".status{padding:10px;margin:10px 0;border-radius:5px;display:none}"
".success{background:#d4edda;color:#155724;border:1px solid #c3e6cb}"
".error{background:#f8d7da;color:#721c24;border:1px solid #f5c6cb}"
"</style></head><body>"
"<div class='container'><h1>ESP32 Clock WiFi Setup</h1>"
"<button class='scan-btn' onclick='scanWiFi()'>Scan WiFi Networks</button>"
"<div id='networks'></div>"
"<form id='wifiForm' onsubmit='submitWiFi(event)'>"
"<input type='hidden' id='ssid' name='ssid'>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi Password' required>"
"<button type='submit' class='submit-btn'>Connect to WiFi</button>"
"</form><div id='status' class='status'></div></div>"
"<script>"
"let selectedSSID='';"
"function scanWiFi(){"
"document.getElementById('networks').innerHTML='<p>Scanning...</p>';"
"fetch('/scan').then(r=>r.json()).then(data=>{"
"let html='';"
"data.forEach(ap=>{"
"let security=ap.auth==0?'Open':'Secured';"
"let strength=ap.rssi>-50?'Excellent':ap.rssi>-60?'Good':ap.rssi>-70?'Fair':'Weak';"
"html+=`<div class='network' onclick='selectNetwork(\"${ap.ssid}\")'>`;"
"html+=`<strong>${ap.ssid}</strong>`;"
"html+=`<small>Signal: ${strength} | ${security}</small></div>`;"
"});"
"document.getElementById('networks').innerHTML=html;"
"}).catch(e=>{document.getElementById('networks').innerHTML='<p>Scan failed</p>';});"
"}"
"function selectNetwork(ssid){"
"selectedSSID=ssid;"
"document.getElementById('ssid').value=ssid;"
"document.getElementById('password').focus();"
"document.querySelectorAll('.network').forEach(n=>n.style.borderColor='transparent');"
"event.target.closest('.network').style.borderColor='#4CAF50';"
"}"
"function submitWiFi(e){"
"e.preventDefault();"
"let formData=new FormData(e.target);"
"let status=document.getElementById('status');"
"status.style.display='block';"
"status.className='status';"
"status.textContent='Connecting...';"
"fetch('/connect',{method:'POST',body:new URLSearchParams(formData)})"
".then(r=>r.text()).then(data=>{"
"status.className='status success';"
"status.textContent='WiFi credentials saved! Device will reconnect...';"
"setTimeout(()=>location.reload(),3000);"
"}).catch(e=>{"
"status.className='status error';"
"status.textContent='Failed to save credentials';"
"});"
"}"
"</script></body></html>";

// Handler for root page
static esp_err_t config_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, config_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// Handler for WiFi scan
static esp_err_t scan_get_handler(httpd_req_t *req) {
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false
    };
    
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    
    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    
    if (ap_count == 0) {
        httpd_resp_sendstr(req, "[]");
        return ESP_OK;
    }
    
    wifi_ap_record_t *ap_list = malloc(sizeof(wifi_ap_record_t) * ap_count);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));
    
    // Build JSON response
    char *json = malloc(ap_count * 150);
    strcpy(json, "[");
    
    for (int i = 0; i < ap_count; i++) {
        char ap_json[150];
        snprintf(ap_json, sizeof(ap_json),
                "%s{\"ssid\":\"%s\",\"rssi\":%d,\"auth\":%d}",
                i > 0 ? "," : "",
                ap_list[i].ssid,
                ap_list[i].rssi,
                ap_list[i].authmode);
        strcat(json, ap_json);
    }
    strcat(json, "]");
    
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);
    
    free(ap_list);
    free(json);
    return ESP_OK;
}

// Handler for WiFi connection
static esp_err_t connect_post_handler(httpd_req_t *req) {
    char buf[200];
    int ret, remaining = req->content_len;
    
    if (remaining >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Content too long");
        return ESP_FAIL;
    }
    
    ret = httpd_req_recv(req, buf, remaining);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
        return ESP_FAIL;
    }
    buf[ret] = '\0';
    
    // Parse form data
    char ssid[33] = {0};
    char password[65] = {0};
    
    char *ssid_start = strstr(buf, "ssid=");
    char *pass_start = strstr(buf, "password=");
    
    if (ssid_start && pass_start) {
        ssid_start += 5;
        char *ssid_end = strchr(ssid_start, '&');
        if (ssid_end) {
            int len = ssid_end - ssid_start;
            if (len < sizeof(ssid)) {
                strncpy(ssid, ssid_start, len);
                ssid[len] = '\0';
            }
        }
        
        pass_start += 9;
        int len = strlen(pass_start);
        if (len < sizeof(password)) {
            strncpy(password, pass_start, len);
            password[len] = '\0';
        }
        
        // URL decode (basic)
        for(char *p = ssid; *p; p++) if(*p == '+') *p = ' ';
        for(char *p = password; *p; p++) if(*p == '+') *p = ' ';
        
        ESP_LOGI(TAG_WEB, "Received SSID: %s", ssid);
        
        // Save credentials
        save_wifi_credentials(ssid, password);
        
        // Update WiFi config
        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, ssid);
        strcpy((char *)wifi_config.sta.password, password);
        wifi_config.sta.threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD;
        wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PK_MODE_AUTOMATIC;
        
        esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
        esp_wifi_disconnect();
        vTaskDelay(100 / portTICK_PERIOD_MS);
        esp_wifi_connect();
        
        httpd_resp_sendstr(req, "OK");
        return ESP_OK;
    }
    
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid data");
    return ESP_FAIL;
}

// Start web server
void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port = 80;
    
    ESP_LOGI(TAG_WEB, "Starting web server on port %d", config.server_port);
    
    if (httpd_start(&webserver, &config) == ESP_OK) {
        httpd_uri_t config_uri = {
            .uri = "/",
            .method = HTTP_GET,
            .handler = config_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(webserver, &config_uri);
        
        httpd_uri_t scan_uri = {
            .uri = "/scan",
            .method = HTTP_GET,
            .handler = scan_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(webserver, &scan_uri);
        
        httpd_uri_t connect_uri = {
            .uri = "/connect",
            .method = HTTP_POST,
            .handler = connect_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(webserver, &connect_uri);
        
        ESP_LOGI(TAG_WEB, "Web server started successfully");
        ESP_LOGI(TAG_WEB, "Connect to ESP32_Clock_Config and navigate to http://192.168.4.1");
    } else {
        ESP_LOGE(TAG_WEB, "Failed to start web server");
    }
}

void stop_webserver(void) {
    if (webserver) {
        httpd_stop(webserver);
        webserver = NULL;
    }
}

// Save WiFi credentials to NVS
void save_wifi_credentials(const char* ssid, const char* password) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        nvs_set_str(nvs_handle, "ssid", ssid);
        nvs_set_str(nvs_handle, "password", password);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
        
        strcpy(saved_ssid, ssid);
        strcpy(saved_password, password);
        
        ESP_LOGI(TAG_WEB, "WiFi credentials saved to NVS");
    }
}

// Load WiFi credentials from NVS
void load_wifi_credentials(void) {
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("wifi_config", NVS_READONLY, &nvs_handle);
    if (err == ESP_OK) {
        size_t ssid_len = sizeof(saved_ssid);
        size_t pass_len = sizeof(saved_password);
        
        nvs_get_str(nvs_handle, "ssid", saved_ssid, &ssid_len);
        nvs_get_str(nvs_handle, "password", saved_password, &pass_len);
        nvs_close(nvs_handle);
        
        ESP_LOGI(TAG_WEB, "Loaded WiFi credentials from NVS");
    }
}

// Initialize WiFi in AP+STA mode (hotspot + client)
void wifi_init_apsta(void) {
    // Event loop and netif already initialized by wifi_init_sta, reuse them
    
    // Stop existing WiFi to reconfigure
    esp_wifi_stop();
    esp_wifi_deinit();
    
    // Create AP interface (STA already exists)
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Load saved credentials
    load_wifi_credentials();

    // Configure AP
    wifi_config_t ap_config = {
        .ap = {
            .ssid = ESP_WIFI_AP_SSID,
            .ssid_len = strlen(ESP_WIFI_AP_SSID),
            .channel = ESP_WIFI_AP_CHANNEL,
            .password = ESP_WIFI_AP_PASS,
            .max_connection = ESP_WIFI_AP_MAX_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };

    // Configure STA with saved credentials
    wifi_config_t sta_config = {
        .sta = {
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PK_MODE_AUTOMATIC
        },
    };
    strcpy((char *)sta_config.sta.ssid, saved_ssid);
    strcpy((char *)sta_config.sta.password, saved_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_AP, "WiFi AP started. SSID:%s password:%s channel:%d",
             ESP_WIFI_AP_SSID, ESP_WIFI_AP_PASS, ESP_WIFI_AP_CHANNEL);
    ESP_LOGI(TAG_STATION, "WiFi STA initialized, will retry connection periodically");
    
    ap_mode_active = true;
    
    // Start web server for configuration
    start_webserver();
}

// Task to periodically try reconnecting to saved WiFi
void wifi_reconnect_task(void *pvParameters) {
    ESP_LOGI(TAG_STATION, "WiFi reconnect task started");
    
    while(1) {
        if(!sta_connected && ap_mode_active) {
            ESP_LOGI(TAG_STATION, "Attempting to connect to saved WiFi...");
            esp_wifi_connect();
            
            // Wait 10 seconds to see if connection succeeds
            vTaskDelay(10000 / portTICK_PERIOD_MS);
            
            if(sta_connected) {
                ESP_LOGI(TAG_STATION, "Successfully connected to WiFi!");
                // Optionally disable AP mode after successful connection
                // esp_wifi_set_mode(WIFI_MODE_STA);
                // ap_mode_active = false;
            }
        } else if(sta_connected) {
            // Already connected, just monitor
            vTaskDelay(30000 / portTICK_PERIOD_MS);
        }
        
        // Retry every 30 seconds if not connected
        if(!sta_connected) {
            vTaskDelay(20000 / portTICK_PERIOD_MS);
        }
    }
}