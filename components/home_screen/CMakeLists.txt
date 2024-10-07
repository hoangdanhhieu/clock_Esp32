idf_component_register(SRCS "home_screen.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event esp_wifi ST7789V_LCD icon_font define AHT30 MPU6050)
                    