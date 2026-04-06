#include "color.h"
#include "esp_attr.h"

// Enable color designer mode (comment out to disable)


#ifdef ENABLE_COLOR_DESIGNER
#include <stdio.h>
#include <string.h>
#endif


#include "math.h"
#include "stdio.h"

color_struct sun_color        = {31, 0, 0};
color_struct moon_color       = {31, 63, 31};
color_struct cloud_color      = {25, 50, 25}; // Light Grey for better visibility
color_struct clock_color      = {31, 45, 0};  // Golden Amber for high contrast

color_struct wifi_color       = {31, 63, 31};
color_struct date_color       = {31, 63, 31};
color_struct bg_color         = {7, 38, 31};
color_struct red_color        = {31, 0, 0};
color_struct green_color      = {0, 63, 0};
color_struct white_color      = {31, 63, 31};
color_struct orange_color     = {31, 46, 0};
color_struct yellow_color     = {27, 63, 0};


color_struct switch_color     = {31, 63, 31};
color_struct select_bg        = {0, 18, 0};
color_struct text_clock       = {31, 63, 31};

color_struct light_color      = {0, 9, 11};
color_struct night_color      = {0, 0, 0};  // Pure black for night background
color_struct alarm_ring_color = {19, 23, 18};

color_struct cyan_color       = {0, 63, 31};
color_struct grey_color       = {15, 30, 15};
color_struct dark_cyan_color  = {0, 25, 31};
color_struct bird_body_color  = {0, 50, 31};
color_struct bird_wing_color  = {0, 40, 25};

struct KeyColor {
    float hour;    
    uint8_t r, g, b;             // Sky Color
    uint8_t c_r, c_g, c_b;       // Cloud Color
    uint8_t clk_r, clk_g, clk_b; // Clock Color
    uint8_t txt_r, txt_g, txt_b; // Text/Date Color
    uint8_t bb_r, bb_g, bb_b;    // Bird Body Color
    uint8_t bw_r, bw_g, bw_b;    // Bird Wing Color
};

const struct KeyColor keyframes[] = {
    // Hour,  Sky(R,G,B),        Cloud(R,G,B),      Clock(R,G,B),      Text(R,G,B),       BirdBody(R,G,B),   BirdWing(R,G,B)
    {0.0,     5,   5,   15,      150,  150,  150,      255, 180, 0,       255, 180, 0,       0,   100, 100,     0,   80,  80},    // 00:00 - Deep Night
    {5.0,     20,  20,  50,      150,  150,  150,      255, 180, 0,       255, 180, 0,       0,   100, 100,     0,   80,  80},    // 05:00 - Dawn
    {6.0,     255, 100, 50,      255, 220, 180,     50,  20,  0,       50,  20,  0,       100, 50,  20,      80,  40,  10},    // 06:00 - Sunrise (Lighter clouds)
    {8.0,     135, 206, 235,     255, 255, 255,     0,   50,  50,      0,   50,  50,      0,   200, 200,     0,   160, 160},   // 08:00 - Morning
    {12.0,    80,  160, 255,     255, 255, 255,     0,   0,   0,       0,   0,   0,       0,   255, 255,     0,   200, 200},   // 12:00 - Noon (Blue Sky, White Clouds)
    {17.0,    135, 206, 235,     255, 255, 255,     0,   50,  50,      0,   50,  50,      0,   200, 200,     0,   160, 160},   // 17:00 - Afternoon
    {18.0,    255, 140, 0,       255, 200, 100,     50,  0,   0,       50,  0,   0,       100, 0,   50,      80,  0,   40},    // 18:00 - Sunset (Lighter clouds)
    {18.8,    128, 0,   128,     180, 100, 180,     255, 255, 255,     255, 255, 255,     50,  0,   100,     40,  0,   80},    // 18:45 - Twilight (Lighter clouds)
    {19.5,    15,  15,  40,      150, 150,  150,      255, 180, 0,       255, 180, 0,       0,   100, 100,     0,   80,  80},    // 19:30 - Night
    {24.0,    5,   5,   15,      150,  150,  150,      255, 180, 0,       255, 180, 0,       0,   100, 100,     0,   80,  80}     // 24:00 - Loop
};
const int numKeys = sizeof(keyframes) / sizeof(keyframes[0]);

float lerp(float start, float end, float t) {
    return start + (end - start) * t;
}

void getSkyColorRGB565(float currentHour) {
    while (currentHour >= 24.0) currentHour -= 24.0;
    while (currentHour < 0.0) currentHour += 24.0;
    // printf("Current Hour: %.2f\n", currentHour);
    int idx1 = 0, idx2 = 1;
    for (int i = 0; i < numKeys - 1; i++) {
        if (currentHour >= keyframes[i].hour && currentHour < keyframes[i+1].hour) {
            idx1 = i;
            idx2 = i + 1;
            break;
        }
    }

    float timeDiff = keyframes[idx2].hour - keyframes[idx1].hour;
    float t = (currentHour - keyframes[idx1].hour) / timeDiff;

    // Helper to interpolate and assign to color_struct (swapping R/B for display)
    void update_color(color_struct *c, uint8_t r1, uint8_t g1, uint8_t b1, uint8_t r2, uint8_t g2, uint8_t b2, float t) {
        float r = lerp(r1, r2, t);
        float g = lerp(g1, g2, t);
        float b = lerp(b1, b2, t);
        
        // Gamma/Scaling
        uint8_t r_final = (uint8_t)(pow(r / 255.0, 2.0) * 255.0);
        uint8_t g_final = (uint8_t)(pow(g / 255.0, 2.0) * 255.0);
        uint8_t b_final = (uint8_t)(pow(b / 255.0, 2.0) * 255.0);
        
        // Assign to struct (B gets Red, R gets Blue)
        c->b = r_final >> 3;
        c->g = g_final >> 2;
        c->r = b_final >> 3;
    }

    update_color(&bg_color, keyframes[idx1].r, keyframes[idx1].g, keyframes[idx1].b, 
                           keyframes[idx2].r, keyframes[idx2].g, keyframes[idx2].b, t);
                           
    update_color(&cloud_color, keyframes[idx1].c_r, keyframes[idx1].c_g, keyframes[idx1].c_b, 
                              keyframes[idx2].c_r, keyframes[idx2].c_g, keyframes[idx2].c_b, t);
                              
    update_color(&clock_color, keyframes[idx1].clk_r, keyframes[idx1].clk_g, keyframes[idx1].clk_b, 
                              keyframes[idx2].clk_r, keyframes[idx2].clk_g, keyframes[idx2].clk_b, t);

    update_color(&date_color, keyframes[idx1].txt_r, keyframes[idx1].txt_g, keyframes[idx1].txt_b, 
                             keyframes[idx2].txt_r, keyframes[idx2].txt_g, keyframes[idx2].txt_b, t);
    // Sync other text colors
    text_clock = date_color; 
    white_color = date_color; // 'white_color' is often used for generic text

    update_color(&bird_body_color, keyframes[idx1].bb_r, keyframes[idx1].bb_g, keyframes[idx1].bb_b, 
                                  keyframes[idx2].bb_r, keyframes[idx2].bb_g, keyframes[idx2].bb_b, t);
                                  
    update_color(&bird_wing_color, keyframes[idx1].bw_r, keyframes[idx1].bw_g, keyframes[idx1].bw_b, 
                                  keyframes[idx2].bw_r, keyframes[idx2].bw_g, keyframes[idx2].bw_b, t);
}


#ifdef ENABLE_COLOR_DESIGNER

typedef struct {
    const char* name;
    color_struct* color;
} color_entry_t;

static const color_entry_t color_table[] = {
    {"sun", &sun_color},
    {"moon", &moon_color},
    {"cloud", &cloud_color},
    {"clock", &clock_color},
    {"wifi", &wifi_color},
    {"date", &date_color},
    {"bg", &bg_color},
    {"red", &red_color},
    {"green", &green_color},
    {"white", &white_color},
    {"switch", &switch_color},
    {"select_bg", &select_bg},
    {"text_clock", &text_clock},
    {"light", &light_color},
    {"night", &night_color},
    {"alarm_ring", &alarm_ring_color},
};

static const int color_count = sizeof(color_table) / sizeof(color_entry_t);

void color_designer_help(void) {
    printf("\n=== Color Designer Mode ===\n");
    printf("Commands:\n");
    printf("  list                    - List all colors\n");
    printf("  get <name>              - Get color values (e.g., get bg)\n");
    printf("  set <name> <r> <g> <b>  - Set color (e.g., set bg 0 0 0)\n");
    printf("  help                    - Show this help\n");
    printf("\nAvailable colors:\n");
    for(int i = 0; i < color_count; i++) {
        printf("  %s\n", color_table[i].name);
    }
    printf("RGB ranges: R(0-31), G(0-63), B(0-31)\n");
    printf("===========================\n\n");
}

void color_designer_list(void) {
    printf("\nCurrent color values:\n");
    for(int i = 0; i < color_count; i++) {
        printf("  %-12s: R=%2d G=%2d B=%2d\n", 
               color_table[i].name,
               color_table[i].color->r,
               color_table[i].color->g,
               color_table[i].color->b);
    }
    printf("\n");
}

void color_designer_get(const char* name) {
    for(int i = 0; i < color_count; i++) {
        if(strcmp(color_table[i].name, name) == 0) {
            printf("%s: R=%d G=%d B=%d\n", 
                   color_table[i].name,
                   color_table[i].color->r,
                   color_table[i].color->g,
                   color_table[i].color->b);
            return;
        }
    }
    printf("Error: Color '%s' not found\n", name);
}

void color_designer_set(const char* name, int r, int g, int b) {
    // Validate ranges
    if(r < 0 || r > 31 || g < 0 || g > 63 || b < 0 || b > 31) {
        printf("Error: RGB values out of range (R:0-31, G:0-63, B:0-31)\n");
        return;
    }
    
    for(int i = 0; i < color_count; i++) {
        if(strcmp(color_table[i].name, name) == 0) {
            color_table[i].color->r = r;
            color_table[i].color->g = g;
            color_table[i].color->b = b;
            printf("Set %s to R=%d G=%d B=%d\n", name, r, g, b);
            return;
        }
    }
    printf("Error: Color '%s' not found\n", name);
}

void color_designer_process_command(const char* cmd) {
    char buffer[128];
    strncpy(buffer, cmd, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';
    
    // Remove newline
    char* newline = strchr(buffer, '\n');
    if(newline) *newline = '\0';
    newline = strchr(buffer, '\r');
    if(newline) *newline = '\0';
    
    // Parse command
    char* token = strtok(buffer, " ");
    if(!token) return;
    
    if(strcmp(token, "help") == 0) {
        color_designer_help();
    } else if(strcmp(token, "list") == 0) {
        color_designer_list();
    } else if(strcmp(token, "get") == 0) {
        token = strtok(NULL, " ");
        if(token) {
            color_designer_get(token);
        } else {
            printf("Usage: get <color_name>\n");
        }
    } else if(strcmp(token, "set") == 0) {
        char* name = strtok(NULL, " ");
        char* r_str = strtok(NULL, " ");
        char* g_str = strtok(NULL, " ");
        char* b_str = strtok(NULL, " ");
        
        if(name && r_str && g_str && b_str) {
            int r = atoi(r_str);
            int g = atoi(g_str);
            int b = atoi(b_str);
            color_designer_set(name, r, g, b);
        } else {
            printf("Usage: set <color_name> <r> <g> <b>\n");
        }
    } else {
        printf("Unknown command. Type 'help' for available commands.\n");
    }
}

#endif
