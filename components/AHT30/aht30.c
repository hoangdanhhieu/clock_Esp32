#include "aht30.h"
#include "define.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "stdint.h"
#include "esp_log.h"

#define aht30_address 0x38
static const uint8_t measure_command[3] = {0xAC, 0x33, 0x00};
uint8_t Calc_CRC8(unsigned char *message,unsigned char Num);
static i2c_master_dev_handle_t dev_handle;

void init_aht30(i2c_master_bus_handle_t bus_handle){
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = aht30_address,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
}

void aht30_read(float *humi, float *temp){
    uint8_t read_buffer[7];
    uint32_t humidity_raw, temperature_raw;
    i2c_master_transmit(dev_handle, measure_command, 3, -1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    i2c_master_receive(dev_handle, read_buffer, 7, -1);
    if ((read_buffer[0] & 0x80) != 0){
        ESP_LOGI("AHT30_debug", "Measuring in progress");
        vTaskDelay(100 / portTICK_PERIOD_MS);
        i2c_master_receive(dev_handle, read_buffer, 7, -1);
    }
    if(Calc_CRC8(read_buffer, 6) != read_buffer[6]){
        ESP_LOGI("AHT30_debug", "CRC error");
    } else { 
        temperature_raw = ((((uint32_t)read_buffer[3]) << 16) |
                       (((uint32_t)read_buffer[4]) << 8) |
                       (((uint32_t)read_buffer[5]) << 0)) & 0xFFFFF; 
           
        humidity_raw = ((((uint32_t)read_buffer[1]) << 16) |
                    (((uint32_t)read_buffer[2]) << 8) |
                    (((uint32_t)read_buffer[3]) << 0)) >> 4;       

        *humi = (float)humidity_raw/1048576 * 100;
        *temp = (float)temperature_raw/1048576 * 200 - 50;
    }
}


//**********************************************************//
//CRC check type: CRC8
//polynomial: X8+X5+X4+1
//Poly:0011 0001 0x31
uint8_t Calc_CRC8(uint8_t *data, uint8_t len){
    uint8_t i;
    uint8_t byte;
    uint8_t crc = 0xFF;
    for (byte = 0; byte < len; byte++){
        crc ^= (data[byte]);
        for(i = 8; i > 0; --i){
            if((crc & 0x80) != 0)
                crc = (crc << 1) ^ 0x31;
            else
                crc = (crc << 1);
        }
    }
    return crc;
}
//**********************************************************//