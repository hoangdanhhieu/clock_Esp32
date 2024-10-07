idf_component_register(SRCS "ble_gatts_server.c"
                    INCLUDE_DIRS "." REQUIRES driver esp_event bt)
    