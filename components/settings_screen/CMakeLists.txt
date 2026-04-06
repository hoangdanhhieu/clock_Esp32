idf_component_register(SRCS "settings_screen.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event ili9341 icon_font define nvs_flash)
