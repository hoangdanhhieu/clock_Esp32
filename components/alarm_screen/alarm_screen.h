#include "driver/pulse_cnt.h"

extern spi_device_handle_t spi;
extern uint32_t alarm_array[10];
extern uint8_t lcd_buffer[115200];
extern uint8_t key1Status, countUp, countDown;
extern uint8_t curr_screen;
extern SemaphoreHandle_t spi_xSemaphore;
extern pcnt_unit_handle_t pcnt_unit;
void init_alarm();
void stop_alarm();