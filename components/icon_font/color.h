#include <stdint.h>


#ifndef COLOR_H
#define COLOR_H

typedef struct color_rgb{
    uint8_t b;
    uint8_t g;
    uint8_t r;
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
extern color_struct yellow_color;
extern color_struct orange_color;

extern color_struct switch_color;
extern color_struct select_bg;
extern color_struct text_clock;

extern color_struct alarm_ring_color;

extern color_struct cyan_color;
extern color_struct grey_color;
extern color_struct dark_cyan_color;
extern color_struct bird_body_color;
extern color_struct bird_wing_color;

//#define ENABLE_COLOR_DESIGNER
// Color designer function (only available when ENABLE_COLOR_DESIGNER is defined)
#ifdef ENABLE_COLOR_DESIGNER
void color_designer_process_command(const char* cmd);
void color_designer_help(void);
#endif

void getSkyColorRGB565(float currentHour);

#define write_buffer(buffer, x, y, color) do { \
    int index = (y) * 640 + (x) * 2; \
    buffer[index] = ((color).r << 3) | ((color).g >> 3); \
    buffer[index + 1] = ((color).g << 5) | (color).b; \
} while(0)
 
#endif