
#include "driver/i2c_master.h"
void init_mpu6050(i2c_master_bus_handle_t bus_handle);
void read_mpu6050(int16_t* accel_xyz, int16_t* gyro_xyz);