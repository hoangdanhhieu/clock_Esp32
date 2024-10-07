idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event esp_http_client 
                    lwip esp_netif esp-tls esp_wifi nvs_flash esp_timer ST7789V_LCD home_screen alarm_screen icon_font ble_gatts_server bt AHT30 MPU6050 esp_driver_uart)
                    