#ifndef HOME_SCREEN_H
#define HOME_SCREEN_H



#include "stdint.h"
#include "driver/pulse_cnt.h"
#include "define.h"

extern spi_device_handle_t spi; 
extern time_t update_time;
extern SemaphoreHandle_t spi_xSemaphore;
extern SemaphoreHandle_t i2c_xSemaphore;
extern uint8_t curr_screen;
extern uint8_t key1Status, key2Status, key3Status;
extern pcnt_unit_handle_t pcnt_unit;
extern uint8_t lcd_buffer[153600];
extern uint8_t is_night_mode;
void draw_wifi_status();
void display_date();
void display_clock(void * pvParameters);
void init_home();
void stop_home();
void DrawSun();
void DrawCloud();
void DrawAnimation();
void print_temperature_and_humidity();
void redraw_home_static();

#endif // HOME_SCREEN_H