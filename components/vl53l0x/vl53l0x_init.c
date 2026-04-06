#include "vl53l0x_init.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"
i2c_master_dev_handle_t dev_handle;
//ContinuousRanging
VL53L0X_Error vl53l0x_init(i2c_master_bus_handle_t bus_handle, VL53L0X_DEV pMyDevice, VL53L0X_Version_t *pVersion, VL53L0X_DeviceInfo_t *pDeviceInfo, uint8_t address){
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    pMyDevice->I2cHandle = &dev_handle;
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = address,
        .scl_speed_hz = 100000,
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(bus_handle, &dev_cfg, &dev_handle));
	int32_t status_int;

    status_int = VL53L0X_GetVersion(pVersion);
    if (status_int != 0){
        Status = VL53L0X_ERROR_CONTROL_INTERFACE;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);


    if(Status == VL53L0X_ERROR_NONE){
    	Status = VL53L0X_DataInit(pMyDevice); // Data initialization
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if(Status == VL53L0X_ERROR_NONE){
    	vTaskDelay(10 / portTICK_PERIOD_MS);
    	//Status = VL53L0X_SetDeviceAddress(pMyDevice, address);
    	pMyDevice->I2cDevAddr = address;
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if(Status == VL53L0X_ERROR_NONE){
    	Status = VL53L0X_GetDeviceInfo(pMyDevice, pDeviceInfo);
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if(Status == VL53L0X_ERROR_NONE){
        Status = VL53L0X_StaticInit(pMyDevice); // Device Initialization
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    uint32_t refSpadCount;
    uint8_t isApertureSpads;
    uint8_t VhvSettings;
    uint8_t PhaseCal;

    if(Status == VL53L0X_ERROR_NONE){
    	Status = VL53L0X_PerformRefCalibration(pMyDevice, &VhvSettings, &PhaseCal); // Device Initialization
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if(Status == VL53L0X_ERROR_NONE){
    	Status = VL53L0X_PerformRefSpadManagement(pMyDevice, &refSpadCount, &isApertureSpads); // Device Initialization
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);

    if(Status == VL53L0X_ERROR_NONE){
        Status = VL53L0X_SetDeviceMode(pMyDevice, VL53L0X_DEVICEMODE_CONTINUOUS_RANGING); // Setup in single ranging mode
    }
    if (Status == VL53L0X_ERROR_NONE) {
    	Status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(pMyDevice, 20000);
    }
    if (Status == VL53L0X_ERROR_NONE) {
        Status = VL53L0X_SetLimitCheckEnable(pMyDevice,
        		VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE, 1);
    }
    if (Status == VL53L0X_ERROR_NONE) {
        Status = VL53L0X_SetLimitCheckEnable(pMyDevice,
        		VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE, 1);
    }

    if (Status == VL53L0X_ERROR_NONE) {
        Status = VL53L0X_SetLimitCheckValue(pMyDevice,
        		VL53L0X_CHECKENABLE_SIGNAL_RATE_FINAL_RANGE,
        		(FixPoint1616_t)(0.5*65536));
	}
    if (Status == VL53L0X_ERROR_NONE) {
        Status = VL53L0X_SetLimitCheckValue(pMyDevice,
        		VL53L0X_CHECKENABLE_SIGMA_FINAL_RANGE,
        		(FixPoint1616_t)(18*65536));
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
    if(Status == VL53L0X_ERROR_NONE){
    	Status = VL53L0X_StartMeasurement(pMyDevice);
    }
    return Status;
}

VL53L0X_Error vl53l0x_GetRanging_now(VL53L0X_DEV pMyDevice, uint16_t *result) {
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	VL53L0X_RangingMeasurementData_t *pRangingMeasurementData = &RangingMeasurementData;
	VL53L0X_Error Status = VL53L0X_ERROR_NONE;
	Status = WaitMeasurementDataReady(pMyDevice);
	if(Status == VL53L0X_ERROR_NONE){
		Status = VL53L0X_GetRangingMeasurementData(pMyDevice, pRangingMeasurementData);
		VL53L0X_ClearInterruptMask(pMyDevice, VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY);
		VL53L0X_PollingDelay(pMyDevice);
		if(pRangingMeasurementData->RangeStatus == 0){
			*result = pRangingMeasurementData->RangeMilliMeter;
		} else {
			*result = 8000;
		}
	}
	return Status;
}

VL53L0X_Error vl53l0x_GetRanging_last(VL53L0X_DEV pMyDevice, uint16_t *result){
	VL53L0X_RangingMeasurementData_t RangingMeasurementData;
	VL53L0X_RangingMeasurementData_t *pRangingMeasurementData = &RangingMeasurementData;
	VL53L0X_Error Status = VL53L0X_GetRangingMeasurementData(pMyDevice, pRangingMeasurementData);
	*result = pRangingMeasurementData->RangeMilliMeter;
	VL53L0X_ClearInterruptMask(pMyDevice, VL53L0X_REG_SYSTEM_INTERRUPT_GPIO_NEW_SAMPLE_READY);
	VL53L0X_PollingDelay(pMyDevice);
	return Status;
}

VL53L0X_Error WaitStopCompleted(VL53L0X_DEV Dev) {
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    uint32_t StopCompleted=0;
    uint32_t LoopNb;

    // Wait until it finished
    // use timeout to avoid deadlock
    if (Status == VL53L0X_ERROR_NONE) {
        LoopNb = 0;
        do {
            Status = VL53L0X_GetStopCompletedStatus(Dev, &StopCompleted);
            if ((StopCompleted == 0x00) || Status != VL53L0X_ERROR_NONE) {
                break;
            }
            LoopNb = LoopNb + 1;
            VL53L0X_PollingDelay(Dev);

        } while (LoopNb < VL53L0X_DEFAULT_MAX_LOOP);

        if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP) {
            Status = VL53L0X_ERROR_TIME_OUT;
        }

    }

    return Status;
}

VL53L0X_Error WaitMeasurementDataReady(VL53L0X_DEV Dev) {
    VL53L0X_Error Status = VL53L0X_ERROR_NONE;
    uint8_t NewDatReady=0;
    uint32_t LoopNb;

    // Wait until it finished
    // use timeout to avoid deadlock
    if (Status == VL53L0X_ERROR_NONE) {
        LoopNb = 0;
        do {
            Status = VL53L0X_GetMeasurementDataReady(Dev, &NewDatReady);
            if ((NewDatReady == 0x01) || Status != VL53L0X_ERROR_NONE) {
                break;
            }
            LoopNb = LoopNb + 1;
            VL53L0X_PollingDelay(Dev);
        } while (LoopNb < VL53L0X_DEFAULT_MAX_LOOP);

        if (LoopNb >= VL53L0X_DEFAULT_MAX_LOOP) {
            Status = VL53L0X_ERROR_TIME_OUT;
        }
    }

    return Status;
}

