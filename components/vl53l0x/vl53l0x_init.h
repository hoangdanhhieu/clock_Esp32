#ifndef VL53L0X_INIT_H
#define VL53L0X_INIT_H

#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include <stdint.h>
#include <stdbool.h>
VL53L0X_Error vl53l0x_init(i2c_master_bus_handle_t bus_handle, VL53L0X_DEV pMyDevice, VL53L0X_Version_t *pVersion, VL53L0X_DeviceInfo_t *pDeviceInfo, uint8_t address);
VL53L0X_Error vl53l0x_GetRanging_now(VL53L0X_DEV pMyDevice, uint16_t *result);
VL53L0X_Error WaitStopCompleted(VL53L0X_DEV Dev);
VL53L0X_Error WaitMeasurementDataReady(VL53L0X_DEV Dev);
VL53L0X_Error vl53l0x_GetRanging_last(VL53L0X_DEV pMyDevice, uint16_t *result);
VL53L0X_Error vl53l0x_GetRanging2_now(VL53L0X_DEV pMyDevice1, VL53L0X_DEV pMyDevice2,
												uint16_t *result1, uint16_t *result2);
VL53L0X_Error vl53l0x_GetRanging4_now(VL53L0X_DEV pMyDevice1, VL53L0X_DEV pMyDevice2,
											VL53L0X_DEV pMyDevice3, VL53L0X_DEV pMyDevice4,
												uint16_t *result1, uint16_t *result2,
													uint16_t *result3, uint16_t *result4);

#endif // VL53L0X_INIT_H