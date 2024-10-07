#include "stdint.h"
#include "driver/pulse_cnt.h"
extern spi_device_handle_t spi; 
extern time_t update_time;
extern SemaphoreHandle_t spi_xSemaphore;
extern SemaphoreHandle_t i2c_xSemaphore;
extern uint8_t curr_screen;
extern uint8_t key1Status, countUp, countDown;
extern pcnt_unit_handle_t pcnt_unit;
extern uint8_t lcd_buffer[115200];
void draw_wifi_status();
void display_date();
void display_clock(void * pvParameters);
void init_home();
void stop_home();
void DrawSun();
void DrawCloud();
void DrawBird();
void print_temperature_and_humidity();