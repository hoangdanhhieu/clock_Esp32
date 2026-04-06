#ifndef DEFINE_H
#define DEFINE_H
#include "stdint.h"

#define PIN_NUM_SCL 4
#define PIN_NUM_SDA 5
#define PIN_NUM_BUZZER 0


#define PIN_NUM_LED   20
#define PIN_NUM_CLK  19
#define PIN_NUM_MOSI 15
#define PIN_NUM_DC   14
#define PIN_NUM_RS   9
#define PIN_NUM_CS   8

#define PIN_NUM_BTN1 1 
#define PIN_NUM_BTN2 2
#define PIN_NUM_BTN3 3

enum {
    NOTHING, CLICK_KEY, HOLD_KEY
};

enum {  
    HOME_SCREEN,
    ALARM_SCREEN,
    FOCUS_ALARM_SCREEN,
    ALARM_RING,
    SETTINGS_SCREEN,
    FOCUS_SETTINGS_SCREEN
};

// Animation modes for home screen
enum {
    ANIM_BIRD,
    ANIM_TEAPOT,
    ANIM_MODE_COUNT  // Total number of animation modes
};

extern const char *name_alarm[10];
extern uint8_t animation_mode;

#define I2C_TIMEOUT_MS 1000
#endif // DEFINE_H