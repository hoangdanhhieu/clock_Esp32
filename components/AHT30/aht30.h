#include "driver/i2c_master.h"
void init_i2c();
void aht30_read(float *humi, float *temp);
void i2c_probe();
void init_aht30(i2c_master_bus_handle_t bus_handle);