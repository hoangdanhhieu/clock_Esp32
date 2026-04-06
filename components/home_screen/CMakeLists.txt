idf_component_register(SRCS "home_screen.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event esp_wifi ili9341 icon_font define AHT30 mpu6050)
                    