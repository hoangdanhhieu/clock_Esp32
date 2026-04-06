#ifndef SETTINGS_SCREEN_H
#define SETTINGS_SCREEN_H

#include <stdint.h>

void init_settings();
void stop_settings();
void redraw_settings_static();
uint32_t get_alarm_volume();

#endif
