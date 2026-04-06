idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event esp_http_client esp_http_server
                    lwip esp_netif esp-tls esp_wifi nvs_flash esp_timer ili9341 
                    home_screen alarm_screen settings_screen icon_font ble_gatts_server bt AHT30 mpu6050 esp_driver_uart
                    temt6000 vl53l0x)
                    