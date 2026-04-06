#ifndef ALARM_SCREEN_H
#define ALARM_SCREEN_H

#include "driver/pulse_cnt.h"

extern spi_device_handle_t spi;
extern uint32_t alarm_array[10];
extern uint8_t lcd_buffer[153600];
extern uint8_t key1Status, key2Status, key3Status;
extern uint8_t curr_screen;
extern SemaphoreHandle_t spi_xSemaphore;
void init_alarm();
void stop_alarm();
void redraw_alarm_static();

#endif // ALARM_SCREEN_H