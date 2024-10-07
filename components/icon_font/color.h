#include <stdint.h>

typedef struct color_rgb{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_struct;

extern color_struct sun_color;
extern color_struct moon_color;
extern color_struct cloud_color;
extern color_struct clock_color;
extern color_struct wifi_color;
extern color_struct date_color;
extern color_struct bg_color;
extern color_struct red_color;
extern color_struct green_color;
extern color_struct white_color;

extern color_struct switch_color;
extern color_struct select_bg;
extern color_struct text_clock;

extern color_struct light_color;
extern color_struct night_color;
extern color_struct alarm_ring_color;

#define write_buffer(buffer, x, y, color) int index = y * 480 + x * 12 / 8; \
    if(x & 1){ \
        buffer[index] = (buffer[index] & 0xf0) | ((color).r); \
        buffer[index + 1] = ((color).g << 4) | ((color).b); \
    } else { \
        buffer[index] = ((color).r << 4) | ((color).g);\
        buffer[index + 1] = (buffer[index + 1] & 0x0f) | ((color).b << 4); \
    } \
 