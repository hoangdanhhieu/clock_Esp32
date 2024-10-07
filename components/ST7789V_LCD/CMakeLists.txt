idf_component_register(SRCS "st7789.c"
                    INCLUDE_DIRS "." 
                    REQUIRES driver esp_event esp_http_client lwip esp_netif esp-tls esp_wifi nvs_flash icon_font define)
                    