#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_attr.h"
#include "soc/gpio_sig_map.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
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
#include "st7789.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "home_screen.h"
#include "alarm_screen.h"
#include "animated.h"
#include "digit_clock.h"
#include "define.h"
#include "ble_gatts_server.c"
#include "driver/i2c_master.h"
#include "driver/uart.h"
#include "aht30.h"
#include "driver/pulse_cnt.h"
#include "mpu6050.h"

spi_device_handle_t spi;
i2c_master_bus_handle_t bus_handle;
#define ESP_WIFI_SSID ""
#define ESP_WIFI_PASS ""
#define ESP_MAXIMUM_RETRY 4
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK


#define MAX_HTTP_OUTPUT_BUFFER 1000


static const char *TAG_STATION = "wifi station";
static const char *TAG_SNTP = "SNTP";
static const char *TAG_HTTP = "http";
static const char *TAG_timenow = "time now";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

time_t update_time = 10000000;
char http_buffer[1000];
DMA_ATTR uint8_t lcd_buffer[115200];
SemaphoreHandle_t spi_xSemaphore;
SemaphoreHandle_t i2c_xSemaphore;
TaskHandle_t xHandle = NULL;
pcnt_unit_handle_t pcnt_unit = NULL;

uint8_t key1Status, countUp, countDown;
#define holdTimeUs 300000

uint32_t alarm_array[10];
esp_err_t _http_event_handler(esp_http_client_event_t *evt);
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data);

uint8_t curr_screen = HOME_SCREEN;

                    
void wifi_init_sta(void);
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
void init_rotary_encoder();

void app_main(void) {
    spi_xSemaphore = xSemaphoreCreateMutex();
    i2c_xSemaphore = xSemaphoreCreateMutex();
    gpio_init();
    spi2_lcd_init(&spi);
    fill_display(spi, &bg_color);
    xTaskCreate( refresh_screen, "refresh_screen", 1000, NULL, 2, &xHandle );
    configASSERT( xHandle );
    wifi_ap_record_t ap_info;
    buzzer_pwm_init();
    init_i2c();
    init_mpu6050(bus_handle);
    init_aht30(bus_handle);
    show_text(spi, (uint8_t*)"Connecting...", &white_color, &bg_color, 100, 100, char_map30, font_pixels30);
    init_alarm_nvs();
    ESP_LOGI(TAG_STATION, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    init_rotary_encoder();
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    while(err != ESP_OK){
        esp_wifi_connect();
        vTaskDelay(5000 / portTICK_PERIOD_MS);
        err = esp_wifi_sta_get_ap_info(&ap_info);
    }
    init_ble_gatts_server();
    //http_init();
    fill_display(spi, &bg_color);
    show_text(spi, (uint8_t*)"Update time...", &white_color, &bg_color, 100, 100, char_map30, font_pixels30);
    time_init();
    init_home();
    xTaskCreate( scan_key, "keyScan", 2000, NULL, 2, &xHandle );
    configASSERT( xHandle );
    xTaskCreate( alarm_loop, "AlarmLoop", 3000, NULL, 2, &xHandle );
    configASSERT( xHandle );
    xTaskCreate( theme_loop, "ThemeLoop", 2000, NULL, 2, &xHandle );
    configASSERT( xHandle );
    
    while (1) {
        if(curr_screen == HOME_SCREEN || curr_screen == ALARM_SCREEN){
            if (key1Status || countDown || countUp) {
                if(countUp && curr_screen == HOME_SCREEN){
                    stop_home();
                    init_alarm();
                } else if(countDown && curr_screen == ALARM_SCREEN){
                    stop_alarm();
                    init_home();
                }  
                pcnt_unit_clear_count(pcnt_unit);
                key1Status = countDown = countUp = 0;
            }
        }
        vTaskDelay(70 / portTICK_PERIOD_MS);
    }
}

void theme_loop(){
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    uint8_t flag = (timeinfo.tm_hour >= 18 || timeinfo.tm_hour <= 6) ? 0 : 1;
    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);
        if(timeinfo.tm_hour >= 18 || timeinfo.tm_hour <= 6){
            if(flag == 0){
                flag = 1;
                bg_color = night_color;
                stop_screen();
                init_home();
            }
        } else {
            if(flag == 1){
                flag = 0;
                bg_color = light_color;
                stop_screen();
                init_home();
            }
        }
        vTaskDelay(60000 / portTICK_PERIOD_MS);
    }
}

void stop_screen(){
    if(curr_screen == HOME_SCREEN){
        stop_home();
    } else if(curr_screen == ALARM_SCREEN || curr_screen == FOCUS_ALARM_SCREEN){
        stop_alarm();
    }
}

void init_rotary_encoder(){
     pcnt_unit_config_t unit_config = {
        .high_limit = 100,
        .low_limit = -100,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = GPIO_A,
        .level_gpio_num = GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));
    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = GPIO_B,
        .level_gpio_num = GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    //ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
   // ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
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
                while(key1Status && count < 750){
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 5000);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    count++;
                }
                key1Status = 0;
                pcnt_unit_clear_count(pcnt_unit);
                ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
                ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
                fill_display(spi, &bg_color);
                init_home();
                while(hour == timeinfo.tm_hour && minutes == timeinfo.tm_min){
                    time(&now);
                    localtime_r(&now, &timeinfo);
                    vTaskDelay(1000 / portTICK_PERIOD_MS);
                }
            }
        }
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}


void refresh_screen(){
    while(1){
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        lcd_print_data(spi, 0, 319, 0, 59, lcd_buffer, 28800);
        lcd_print_data(spi, 0, 319, 60, 119, lcd_buffer + 28800, 28800);
        lcd_print_data(spi, 0, 319, 120, 179, lcd_buffer + 57600, 28800);
        lcd_print_data(spi, 0, 319, 180, 239, lcd_buffer + 86400, 28800);
        xSemaphoreGive(spi_xSemaphore);
        vTaskDelay(50 / portTICK_PERIOD_MS);
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
    fill_display(spi, &bg_color);
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    for(int y = 10; y < 110; y++){
        for(int x = 110; x < 210; x++){
            v = ringg[100 * (y - 10) + (x - 110)];
            if(v != last_v){
                alpha = (float)v / 255;
                oneminusalpha = 1 - alpha;
                last_color.r = (uint8_t)((alarm_ring_color.r * alpha) + (oneminusalpha * bg_color.r));
                last_color.g = (uint8_t)((alarm_ring_color.g * alpha) + (oneminusalpha * bg_color.g));
                last_color.b = (uint8_t)((alarm_ring_color.b * alpha) + (oneminusalpha * bg_color.b));
                write_buffer(lcd_buffer, x, y, last_color);
                last_v = v;
            } else {
                write_buffer(lcd_buffer, x, y, last_color);
            }
        }
    }
    xSemaphoreGive(spi_xSemaphore);
    for(int i = 0; i < 5; i++){
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        font_c = clock_map[temp_clock[i]];
        Xe = Xs + font_c.w + font_c.left;
        Ye = Ys + font_c.h + font_c.top;
        for(int y = Ys; y < Ye; y++){
            for(int x = Ys; x < Xe; x++){
                write_buffer(lcd_buffer, x, y, bg_color);
            }
            
        }
        for(int y = Ys + font_c.top; Ye; y++){
            for(int x = Xs + font_c.left; x < Xe; x++){
                v = clock_pixels[font_c.offset + (x - Xs + font_c.left) + (y - Ys + font_c.top) * font_c.w];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((white_color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((white_color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((white_color.b * alpha) + (oneminusalpha * bg_color.b));
                    write_buffer(lcd_buffer, x, y, last_color);
                    last_v = v;
                } else {
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
        }
        Xs = Xs + font_c.advance;
        xSemaphoreGive(spi_xSemaphore);
    }
    
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
    int key1;
    bool flag1 = false;
    key1Status = countUp = countDown = 0;
    int64_t last_time = 0;
    int pulse_count = 0;
    while(1){
        if(countUp || countDown || key1Status){
            vTaskDelay(50 / portTICK_PERIOD_MS);
            continue;
        }
        pcnt_unit_get_count(pcnt_unit, &pulse_count);
        key1 = gpio_get_level(PIN_NUM_BTN1);
        countUp = (pulse_count < 0) ? 1 : 0;
        countDown = (pulse_count > 0) ? 1 : 0;
        if(key1 && flag1){
            flag1 = false;
            key1Status = 1;
        }
        if(!key1 || flag1){
            if(flag1){
                if(esp_timer_get_time() - last_time > holdTimeUs){
                    while(!gpio_get_level(PIN_NUM_BTN1)){
                        vTaskDelay(100 / portTICK_PERIOD_MS);
                    }
                    flag1 = false;
                    key1Status = HOLD_KEY;
                }
            } else {
                last_time = esp_timer_get_time();
                flag1 = true;
            }
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
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
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
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
        .sta = {.ssid = ESP_WIFI_SSID,
                .password = ESP_WIFI_PASS,
                .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
                .sae_pwe_h2e = WPA3_SAE_PK_MODE_DISABLED},
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG_STATION, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or
     * connection failed for the maximum number of re-tries (WIFI_FAIL_BIT). The
     * bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
     * can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG_STATION, "connected to ap SSID:%s password:%s", ESP_WIFI_SSID,
                 ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG_STATION, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG_STATION, "UNEXPECTED EVENT");
    }
}

void buzzer_pwm_init() {
    ledc_timer_config_t pwm_tmr_cfg = {.speed_mode = LEDC_LOW_SPEED_MODE,
                                       .duty_resolution = LEDC_TIMER_13_BIT,
                                       .timer_num = LEDC_TIMER_0,
                                       .freq_hz = 3000,
                                       .clk_cfg = LEDC_AUTO_CLK};
    ledc_channel_config_t pwm_chn_cfg = {
        .gpio_num = PIN_NUM_BUZZER,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
    };
    ledc_timer_config(&pwm_tmr_cfg);
    ledc_channel_config(&pwm_chn_cfg);
    //ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 1000);
    //ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
    //vTaskDelay(200 / portTICK_PERIOD_MS);
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}

void gpio_init() {
    gpio_config_t io_conf_lcd = {};
    io_conf_lcd.pin_bit_mask = ((1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RS));
    io_conf_lcd.mode = GPIO_MODE_OUTPUT;
    io_conf_lcd.pull_up_en = true;
    gpio_config(&io_conf_lcd);

    gpio_config_t io_conf_btn = {};
    io_conf_btn.pin_bit_mask = (1ULL << PIN_NUM_BTN1);
    io_conf_btn.mode = GPIO_MODE_INPUT;
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
    time_t now;
    struct tm timeinfo;
    sntp_init_t();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    setenv("TZ", "<+07>-7", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    sntp_set_sync_mode(SNTP_SYNC_MODE_IMMED);
    char strftime_buf[64];
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG_timenow, "The current date/time is: %s", strftime_buf);
}

void sntp_init_t() {
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("time.google.com");
    esp_netif_sntp_init(&config);
    sntp_set_time_sync_notification_cb(SNTP_callback);
    int retry = 0;
    while (sntp_get_sync_status() != SNTP_SYNC_STATUS_COMPLETED) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        retry++;
        ESP_LOGI(TAG_SNTP, "Waiting for system time to be set... (%d)", retry);
        if(retry > 10) break;
    }
}

void init_i2c(){
    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = -1,
        .scl_io_num = PIN_NUM_SCL,
        .sda_io_num = PIN_NUM_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };  
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &bus_handle));
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
