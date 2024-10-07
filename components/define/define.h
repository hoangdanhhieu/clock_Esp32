#include "stdint.h"

#define PIN_NUM_SCL 5
#define PIN_NUM_SDA 4
#define PIN_NUM_BUZZER 3
#define PIN_NUM_BTN1 2

#define PIN_NUM_CLK  19
#define PIN_NUM_MOSI 18
#define PIN_NUM_RS   15
#define PIN_NUM_DC   14
#define PIN_NUM_CS   8

#define GPIO_A 0
#define GPIO_B 1

#define PIN_NUM_DRDY 20

#define CLICK_KEY 1
#define HOLD_KEY 2

enum {  
    HOME_SCREEN,
    ALARM_SCREEN,
    FOCUS_ALARM_SCREEN,
    ALARM_RING
};
extern const char *name_alarm[10];