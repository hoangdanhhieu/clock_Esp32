#ifndef MPU6050_H
#define MPU6050_H

#include "driver/i2c_master.h"

extern i2c_master_bus_handle_t i2c_master_bus_handle;


typedef struct {

    int16_t Accel_X_RAW;
    int16_t Accel_Y_RAW;
    int16_t Accel_Z_RAW;
    double Ax;
    double Ay;
    double Az;

    int16_t Gyro_X_RAW;
    int16_t Gyro_Y_RAW;
    int16_t Gyro_Z_RAW;
    double Gx;
    double Gy;
    double Gz;

    float Temperature;

    double ComplementaryAngleX;
    double ComplementaryAngleY;
} MPU6050_t;


// Complementary filter structure
typedef struct {
    double alpha;  // Filter coefficient (0-1), higher = trust gyro more
    double angle;  // Current filtered angle
} Complementary_t;


uint8_t init_mpu6050(i2c_master_bus_handle_t bus_handle);
void MPU6050_Read_All(MPU6050_t *DataStruct);
double Complementary_getAngle(Complementary_t *Filter, double accelAngle, double gyroRate, double dt);
void remove_mpu6050();
void MPU6050_WHO_AM_I();

#endif // MPU6050_H