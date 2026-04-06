#include <mpu6050.h>
#include <math.h>
#include "esp_timer.h"
#include "esp_attr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "define.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"
#define RAD_TO_DEG 57.295779513082320876798154814105

#define WHO_AM_I_REG 0x75
#define PWR_MGMT_1_REG 0x6B
#define SMPLRT_DIV_REG 0x19
#define ACCEL_CONFIG_REG 0x1C
#define ACCEL_XOUT_H_REG 0x3B
#define TEMP_OUT_H_REG 0x41
#define GYRO_CONFIG_REG 0x1B
#define GYRO_XOUT_H_REG 0x43
#define FIFO_ADRESS 0x74
#define MPU6050_ADDRESS 0x68

static i2c_master_dev_handle_t i2c_handle;

static int64_t timer;

static Complementary_t ComplementaryX = {
        .alpha = 0.98,  // 98% gyro, 2% accel
        .angle = 0.0
};

static Complementary_t ComplementaryY = {
        .alpha = 0.98,  // 98% gyro, 2% accel
        .angle = 0.0
};

const uint16_t i2c_timeout = 100;

uint8_t init_mpu6050(i2c_master_bus_handle_t bus_handle) {
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = MPU6050_ADDRESS,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &i2c_handle));

    uint8_t check;  
    uint8_t command[2];

    // check device ID WHO_AM_I
    command[0] = WHO_AM_I_REG;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_handle, command, 1, &check, 1, i2c_timeout));
    printf("MPU6050 WHO_AM_I = %d\n", check);
    if (check == 104)  // 0x68 will be returned by the sensor if everything goes well
    {
        // power management register 0X6B we should write all 0's to wake the sensor up
        command[0] = PWR_MGMT_1_REG; command[1] = 0x01;
        i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);
        
        // Set DATA RATE of 1KHz by writing SMPLRT_DIV register
        command[0] = SMPLRT_DIV_REG; command[1] = 19; //100Hz
        i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);

        // Set DLPF (Digital Low Pass Filter)
        command[0] = 0x1A; command[1] = 0x03;  // DLPF_CFG = 3 (44Hz)
        i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);

        // Set accelerometer configuration in ACCEL_CONFIG Register
        // 8g
        command[0] = ACCEL_CONFIG_REG; command[1] = 0x10;
        i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);

        // Set Gyroscopic configuration in GYRO_CONFIG Register
        // 1000d/s
        command[0] = GYRO_CONFIG_REG; command[1] = 0x10;
        i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);
        
        timer = esp_timer_get_time();
        return 0;
    }
    return 1;
}

void remove_mpu6050(){
    uint8_t command[2];
    command[0] = 0x6B; command[1] = 0x80;
    i2c_master_transmit(i2c_handle, command, 2, i2c_timeout);
    ESP_ERROR_CHECK(i2c_master_bus_rm_device(i2c_handle));
}

void MPU6050_WHO_AM_I() {
    uint8_t command = WHO_AM_I_REG;
    uint8_t check;
    ESP_ERROR_CHECK(i2c_master_transmit_receive(i2c_handle, &command, 1, &check, 1, i2c_timeout));
    printf("WHO_AM_I = %d\n", check);
    command = PWR_MGMT_1_REG;
    i2c_master_transmit_receive(i2c_handle, &command, 1, &check, 1, i2c_timeout);
    if (check & 0x40) { // Bit 6 = 1 là Sleep
        printf("MPU6050 is in Sleep mode, waking up...\n");
    } else {
        printf("MPU6050 is awake and operational.\n");
    }
}

void IRAM_ATTR MPU6050_Read_All(MPU6050_t *DataStruct) {
    uint8_t Rec_Data[14];
    int16_t temp;

    // Read 14 BYTES of data starting from ACCEL_XOUT_H register
    uint8_t command = ACCEL_XOUT_H_REG;
    esp_err_t ret = i2c_master_transmit_receive(i2c_handle, &command, 1, Rec_Data, 14, i2c_timeout);
    
    // If I2C read failed, mark angles as invalid (NaN) to trigger recovery
    if(ret != ESP_OK) {
        DataStruct->ComplementaryAngleX = NAN;
        DataStruct->ComplementaryAngleY = NAN;
        return;
    }

    DataStruct->Accel_X_RAW = (int16_t) (Rec_Data[0] << 8 | Rec_Data[1]);
    DataStruct->Accel_Y_RAW = (int16_t) (Rec_Data[4] << 8 | Rec_Data[5]);  // Swap: was Z
    DataStruct->Accel_Z_RAW = (int16_t) (Rec_Data[2] << 8 | Rec_Data[3]);  // Swap: was Y
    temp = (int16_t) (Rec_Data[6] << 8 | Rec_Data[7]);
    DataStruct->Gyro_X_RAW = (int16_t) (Rec_Data[8] << 8 | Rec_Data[9]);
    DataStruct->Gyro_Y_RAW = (int16_t) (Rec_Data[12] << 8 | Rec_Data[13]);  // Swap: was Z
    DataStruct->Gyro_Z_RAW = (int16_t) (Rec_Data[10] << 8 | Rec_Data[11]);  // Swap: was Y

    DataStruct->Ax = (double)DataStruct->Accel_X_RAW / 4100.0;
    DataStruct->Ay = (double)DataStruct->Accel_Y_RAW / 4096.0;
    DataStruct->Az = (double)DataStruct->Accel_Z_RAW / 4300.0;
    DataStruct->Temperature = (double)((int16_t)temp / (double)340.0 + (double)36.53);
    DataStruct->Gx = (double)DataStruct->Gyro_X_RAW / 32.8 + 6.1;
    DataStruct->Gy = (double)DataStruct->Gyro_Y_RAW / 32.8 + 2.1;
    DataStruct->Gz = (double)DataStruct->Gyro_Z_RAW / 32.8 - 1.5;

    // Complementary filter angle calculation
    double dt = (double) (esp_timer_get_time() - timer) / 1000000;
    timer = esp_timer_get_time();
    double roll;
    double roll_sqrt = sqrt(
            DataStruct->Accel_X_RAW * DataStruct->Accel_X_RAW + DataStruct->Accel_Z_RAW * DataStruct->Accel_Z_RAW);
    if (roll_sqrt != 0.0) {
        roll = atan(DataStruct->Accel_Y_RAW / roll_sqrt) * RAD_TO_DEG;
    } else {
        roll = 0.0;
    }
    double pitch = atan2(-DataStruct->Accel_X_RAW, DataStruct->Accel_Z_RAW) * RAD_TO_DEG;
    if ((pitch < -90 && DataStruct->ComplementaryAngleY > 90) || (pitch > 90 && DataStruct->ComplementaryAngleY < -90)) {
        ComplementaryY.angle = pitch;
        DataStruct->ComplementaryAngleY = pitch;
    } else {
        DataStruct->ComplementaryAngleY = Complementary_getAngle(&ComplementaryY, pitch, DataStruct->Gy, dt);
    }
    if (fabs(DataStruct->ComplementaryAngleY) > 90)
        DataStruct->Gx = -DataStruct->Gx;
    DataStruct->ComplementaryAngleX = Complementary_getAngle(&ComplementaryX, roll, DataStruct->Gx, dt);

}

double IRAM_ATTR Complementary_getAngle(Complementary_t *Filter, double accelAngle, double gyroRate, double dt) {
    // Integrate gyroscope to get angle
    double gyroAngle = Filter->angle + gyroRate * dt;
    
    // Complementary filter: combine gyro (high-pass) and accel (low-pass)
    // angle = alpha * (angle + gyro * dt) + (1 - alpha) * accel_angle
    Filter->angle = Filter->alpha * gyroAngle + (1.0 - Filter->alpha) * accelAngle;
    
    return Filter->angle;
};
