#include "mpu6050.h"
#include "define.h"
#include "inttypes.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#define mpu6050_address 0x68


static i2c_master_dev_handle_t dev_handle;
static const uint8_t command_init[][2] = {
    {0x6B, 0x00},
    {0x1A, 0x03},
    {0x1B, 0x08},
    {0x1C, 0x08}
};
void get_id_mpu6050();
void init_mpu6050(i2c_master_bus_handle_t bus_handle){
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = mpu6050_address,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
    get_id_mpu6050();
    for(int i = 0; i < 4; i++){
        ESP_ERROR_CHECK(i2c_master_transmit(dev_handle, command_init[i], 2, -1));
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void read_mpu6050(int16_t* accel_xyz, int16_t* gyro_xyz){
    uint8_t accel_data[6];
    uint8_t gyro_data[6];
    const uint8_t read_accel[1] = {0x3B};
    const uint8_t read_gyro[1] = {0x43};
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, read_accel, sizeof(read_accel), accel_data, 6, -1));
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, read_gyro, sizeof(read_gyro), gyro_data, 6, -1));
    accel_xyz[0] = ((int16_t)accel_data[1]) | ((int16_t)accel_data[0] << 8);
    accel_xyz[1] = ((int16_t)accel_data[3]) | ((int16_t)accel_data[2] << 8);
    accel_xyz[2] = ((int16_t)accel_data[5]) | ((int16_t)accel_data[4] << 8);

    gyro_xyz[0] = ((int16_t)gyro_data[1]) | ((int16_t)gyro_data[0] << 8);
    gyro_xyz[1] = ((int16_t)gyro_data[3]) | ((int16_t)gyro_data[2] << 8);
    gyro_xyz[2] = ((int16_t)gyro_data[5]) | ((int16_t)gyro_data[4] << 8);
}
void get_id_mpu6050(){
    const uint8_t read_command[1] = {0x75};
    uint8_t data[1];
    ESP_ERROR_CHECK(i2c_master_transmit_receive(dev_handle, read_command, sizeof(read_command), data, 1, -1));
    printf("%" PRIu8 "\n", data[0]);
}