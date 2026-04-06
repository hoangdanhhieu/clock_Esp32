#include "digit_clock.h"
#include "icon_screen1.h"
#include "ili9341.h"
#include "esp_wifi.h"
#include <sys/time.h>
#include "home_screen.h"
#include "string.h"
#include "animated.h"
#include "define.h"
#include "aht30.h"
#include "mpu6050.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>

// Global animation mode selector
uint8_t animation_mode = ANIM_TEAPOT;  // Change this to switch animations

volatile bool g_force_clock_redraw = false;
static float last_temp = 0;
static float last_humi = 0;

uint8_t temp_clock[8];
uint8_t buffer_date[] = {'T', '_', ' ', ' ', ' ', 'd', 'd', ' ', '-', ' ','m', 'm', ' ' ,'-' , ' ', 'y', 'y', 'y', 'y', '\0'};


#define STACK_SIZE_DrawClock 2000
StaticTask_t xTaskBuffer_DrawClock;
StackType_t xStack_DrawClock[ STACK_SIZE_DrawClock ];

#define STACK_SIZE_WifiStatus 2000
StaticTask_t xTaskBuffer_WifiStatus;
StackType_t xStack_WifiStatus[ STACK_SIZE_WifiStatus ];

#define STACK_SIZE_DrawSun 1000
StaticTask_t xTaskBuffer_DrawSun;
StackType_t xStack_DrawSun[ STACK_SIZE_DrawSun ];

#define STACK_SIZE_DrawCloud 1000
StaticTask_t xTaskBuffer_DrawCloud;
StackType_t xStack_DrawCloud[ STACK_SIZE_DrawCloud ];

#define STACK_SIZE_DrawBird 3000
StaticTask_t xTaskBuffer_DrawBird;
StackType_t xStack_DrawBird[ STACK_SIZE_DrawBird ];

#define STACK_SIZE_PrintTH 3000
StaticTask_t xTaskBuffer_PrintTH;
StackType_t xStack_PrintTH[ STACK_SIZE_PrintTH ];

TaskHandle_t xHandle_clock;
TaskHandle_t xHandle_WifiStrength;
TaskHandle_t xHandle_DrawSun;
TaskHandle_t xHandle_DrawCloud;
TaskHandle_t xHandle_DrawBird;
TaskHandle_t xHandle_PrintTH;

void init_home(){
    curr_screen = HOME_SCREEN;
    fill_display(&bg_color);
    display_date();

    xHandle_clock = xTaskCreateStatic(
                     display_clock, "Draw clock",
                     STACK_SIZE_DrawClock,  ( void * ) 2, 
                     2, xStack_DrawClock, &xTaskBuffer_DrawClock );
    configASSERT( xHandle_clock );

    xHandle_WifiStrength = xTaskCreateStatic(
                     draw_wifi_status, "Draw WifiStrength",
                     STACK_SIZE_WifiStatus, NULL, 
                     2, xStack_WifiStatus, &xTaskBuffer_WifiStatus );
    configASSERT( xHandle_WifiStrength );

    xHandle_DrawSun = xTaskCreateStatic(
                     DrawSun, "Draw DrawSun",
                     STACK_SIZE_DrawSun, NULL, 
                     2, xStack_DrawSun, &xTaskBuffer_DrawSun );
    configASSERT( xHandle_DrawSun );

    xHandle_DrawCloud = xTaskCreateStatic(
                     DrawCloud, "Draw DrawCloud",
                     STACK_SIZE_DrawCloud, NULL, 
                     2, xStack_DrawCloud, &xTaskBuffer_DrawCloud );
    configASSERT( xHandle_DrawCloud );

    xHandle_DrawBird = xTaskCreateStatic(
                     DrawAnimation, "Draw Animation",
                     STACK_SIZE_DrawBird, NULL, 
                     2, xStack_DrawBird, &xTaskBuffer_DrawBird );
    configASSERT( xHandle_DrawBird );

    xHandle_PrintTH = xTaskCreateStatic(
                     print_temperature_and_humidity, "Draw PrintTH",
                     STACK_SIZE_PrintTH, NULL, 
                     2, xStack_PrintTH, &xTaskBuffer_PrintTH );
    configASSERT( xHandle_PrintTH );
}

void stop_home(){
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    xSemaphoreTake(i2c_xSemaphore, portMAX_DELAY);
    vTaskDelete(xHandle_clock);
    vTaskDelete(xHandle_WifiStrength);
    vTaskDelete(xHandle_DrawSun);
    vTaskDelete(xHandle_DrawCloud);
    vTaskDelete(xHandle_DrawBird);
    vTaskDelete(xHandle_PrintTH);
    xSemaphoreGive(i2c_xSemaphore);
    xSemaphoreGive(spi_xSemaphore);
}

void DrawSun(){
    int i = 0;
    float alpha, oneminusalpha;
    uint8_t v;
    color_struct color;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    uint8_t flag = 255;
    color_struct last_color;
    uint16_t last_v = 256;
    const uint8_t *anim_data = is_night_mode ? moonn : sunn;
    
    while(1){
        time(&now);
        localtime_r(&now, &timeinfo);
        if(is_night_mode != flag){
            flag = is_night_mode;
            color = is_night_mode ? moon_color : sun_color;
            last_v = 256;  // Reset color cache when switching
            anim_data = is_night_mode ? moonn : sunn;
        }
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        
        int anim_offset = i * 2500;
        for(int y = 0; y < 50; y++) {
            int buffer_base_idx = (5 + y) * 640 + 10; // 10 = 5 * 2 (x offset)
            int anim_base_idx = anim_offset + y * 50;
            
            for(int x = 0; x < 50; x++) {
                v = 255 - anim_data[anim_base_idx + x];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((alpha * color.r) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((alpha * color.g) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((alpha * color.b) + (oneminusalpha * bg_color.b));
                    last_v = v;
                }
                
                int index = buffer_base_idx + x * 2;
                lcd_buffer[index] = (last_color.r << 3) | (last_color.g >> 3);
                lcd_buffer[index + 1] = (last_color.g << 5) | last_color.b;
            }
        }
        
        xSemaphoreGive(spi_xSemaphore);
        i = (i + 1) % 28;
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
}

// Animation mode: Procedural tech bird with flapping wings
void DrawBirdAnimation() {
    MPU6050_t mpu_data;
    int frame = 0;
    
    // Bird parameters
    const int16_t bird_center_x = 255;
    const int16_t bird_center_y = 195;
    
    while(1){
        // Read MPU6050 orientation
        xSemaphoreTake(i2c_xSemaphore, I2C_TIMEOUT_MS);
        MPU6050_Read_All(&mpu_data);
        xSemaphoreGive(i2c_xSemaphore);
        
        // Use Z-axis rotation for bird tilt
        float angleZ = -mpu_data.ComplementaryAngleY * 0.0174533;  // Convert to radians and amplify
        float sinZ = sinf(angleZ);
        float cosZ = cosf(angleZ);
        
        // Wing flap animation (0-10 frames, then reverse)
        float wing_phase = (frame < 11) ? frame : (20 - frame);
        float wing_angle = wing_phase * 0.15;  // Smoother flap
        
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        
        // Clear area (70x70 at 220, 160)
        uint8_t bg_hi = (bg_color.r << 3) | (bg_color.g >> 3);
        uint8_t bg_lo = (bg_color.g << 5) | bg_color.b;
        
        for(int y = 0; y < 70; y++) {
            int row_idx = (160 + y) * 640 + 440; // 440 = 220 * 2
            for(int x = 0; x < 70; x++) {
                int idx = row_idx + x * 2;
                lcd_buffer[idx] = bg_hi;
                lcd_buffer[idx + 1] = bg_lo;
            }
        }
        
        // Draw larger eagle-style bird
        
        // Draw wings first (behind body) - larger wingspan
        // Left wing - upper sweep
        for(float t = 0; t <= 1.0; t += 0.01){
            float t2 = t * t;
            float mt = 1 - t;
            float mt2 = mt * mt;
            
            // Wider, more dramatic wing sweep
            int16_t x = -8 * mt2 + (-35 - 15 * sinf(wing_angle)) * 2 * mt * t + (-45) * t2;
            int16_t y = 0 * mt2 + (-25 - 10 * cosf(wing_angle)) * 2 * mt * t + (-12) * t2;
            
            int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
            int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
            
            if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                write_buffer(lcd_buffer, rx, ry, bird_wing_color);
                if(rx + 1 < 290) write_buffer(lcd_buffer, rx + 1, ry, bird_wing_color);
            }
        }
        
        // Left wing - lower sweep
        for(float t = 0; t <= 1.0; t += 0.01){
            float t2 = t * t;
            float mt = 1 - t;
            float mt2 = mt * mt;
            
            int16_t x = -8 * mt2 + (-32 - 12 * sinf(wing_angle)) * 2 * mt * t + (-42) * t2;
            int16_t y = 0 * mt2 + (-18 - 8 * cosf(wing_angle)) * 2 * mt * t + (5) * t2;
            
            int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
            int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
            
            if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                write_buffer(lcd_buffer, rx, ry, bird_wing_color);
            }
        }
        
        // Right wing - upper sweep (mirror)
        for(float t = 0; t <= 1.0; t += 0.01){
            float t2 = t * t;
            float mt = 1 - t;
            float mt2 = mt * mt;
            
            int16_t x = 8 * mt2 + (35 + 15 * sinf(wing_angle)) * 2 * mt * t + 45 * t2;
            int16_t y = 0 * mt2 + (-25 - 10 * cosf(wing_angle)) * 2 * mt * t + (-12) * t2;
            
            int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
            int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
            
            if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                write_buffer(lcd_buffer, rx, ry, bird_wing_color);
                if(rx + 1 < 290) write_buffer(lcd_buffer, rx + 1, ry, bird_wing_color);
            }
        }
        
        // Right wing - lower sweep
        for(float t = 0; t <= 1.0; t += 0.01){
            float t2 = t * t;
            float mt = 1 - t;
            float mt2 = mt * mt;
            
            int16_t x = 8 * mt2 + (32 + 12 * sinf(wing_angle)) * 2 * mt * t + 42 * t2;
            int16_t y = 0 * mt2 + (-18 - 8 * cosf(wing_angle)) * 2 * mt * t + 5 * t2;
            
            int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
            int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
            
            if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                write_buffer(lcd_buffer, rx, ry, bird_wing_color);
            }
        }
        
        // Draw body (streamlined shape)
        // Main body ellipse
        for(int dy = -12; dy <= 12; dy++){
            for(int dx = -8; dx <= 8; dx++){
                // Ellipse equation: (x/a)^2 + (y/b)^2 <= 1
                if((dx*dx)/64.0 + (dy*dy)/144.0 <= 1.0){
                    int16_t rx = bird_center_x + (int16_t)(dx * cosZ - dy * sinZ);
                    int16_t ry = bird_center_y + (int16_t)(dx * sinZ + dy * cosZ);
                    
                    if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                        write_buffer(lcd_buffer, rx, ry, bird_body_color);
                    }
                }
            }
        }
        
        // Draw head (larger oval)
        for(int dy = -6; dy <= 6; dy++){
            for(int dx = -5; dx <= 5; dx++){
                if((dx*dx)/25.0 + (dy*dy)/36.0 <= 1.0){
                    int16_t x = dx;
                    int16_t y = dy - 13;  // Above body
                    
                    int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
                    int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
                    
                    if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                        write_buffer(lcd_buffer, rx, ry, bird_body_color);
                    }
                }
            }
        }
        
        // Draw beak (pointed)
        for(int i = 0; i < 5; i++){
            int16_t x = 0;
            int16_t y = -19 - i;
            
            int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
            int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
            
            if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                write_buffer(lcd_buffer, rx, ry, bird_body_color);
            }
        }
        
        // Draw tail feathers (3 lines)
        for(int offset = -3; offset <= 3; offset += 3){
            for(int i = 0; i < 8; i++){
                int16_t x = offset;
                int16_t y = 12 + i;
                
                int16_t rx = bird_center_x + (int16_t)(x * cosZ - y * sinZ);
                int16_t ry = bird_center_y + (int16_t)(x * sinZ + y * cosZ);
                
                if(rx >= 220 && rx < 290 && ry >= 160 && ry < 230){
                    write_buffer(lcd_buffer, rx, ry, bird_body_color);
                }
            }
        }
        
        xSemaphoreGive(spi_xSemaphore);
        
        frame = (frame + 1) % 21;  // 0-20 for smooth flap cycle
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
}

// Helper macros for anti-aliasing
#define fpart(x) ((x) - floorf(x))
#define rfpart(x) (1.0 - fpart(x))
#define swap(a, b) { float t = a; a = b; b = t; }

static void plot(int x, int y, float c, color_struct color) {
    if (x < 220 || x >= 290 || y < 160 || y >= 230) return;
    
    int index = y * 640 + x * 2;
    uint8_t b0 = lcd_buffer[index];
    uint8_t b1 = lcd_buffer[index + 1];
    
    // Unpack RGB565
    uint8_t bg_r = (b0 >> 3) & 0x1F;
    uint8_t bg_g = ((b0 & 0x07) << 3) | ((b1 >> 5) & 0x07);
    uint8_t bg_b = b1 & 0x1F;
    
    // Blend
    uint8_t r = (uint8_t)(color.r * c + bg_r * (1.0 - c));
    uint8_t g = (uint8_t)(color.g * c + bg_g * (1.0 - c));
    uint8_t b = (uint8_t)(color.b * c + bg_b * (1.0 - c));
    
    // Pack RGB565
    lcd_buffer[index] = (r << 3) | (g >> 3);
    lcd_buffer[index + 1] = (g << 5) | b;
}

static void draw_line_aa(float x0, float y0, float x1, float y1, color_struct color) {
    int steep = fabsf(y1 - y0) > fabsf(x1 - x0);
    
    if (steep) {
        swap(x0, y0);
        swap(x1, y1);
    }
    if (x0 > x1) {
        swap(x0, x1);
        swap(y0, y1);
    }
    
    float dx = x1 - x0;
    float dy = y1 - y0;
    float gradient = (dx == 0.0) ? 1.0 : dy / dx;
    
    // Handle first endpoint
    float xend = roundf(x0);
    float yend = y0 + gradient * (xend - x0);
    float xgap = rfpart(x0 + 0.5);
    int xpxl1 = (int)xend;
    int ypxl1 = (int)yend;
    
    if (steep) {
        plot(ypxl1, xpxl1, rfpart(yend) * xgap, color);
        plot(ypxl1 + 1, xpxl1, fpart(yend) * xgap, color);
    } else {
        plot(xpxl1, ypxl1, rfpart(yend) * xgap, color);
        plot(xpxl1, ypxl1 + 1, fpart(yend) * xgap, color);
    }
    
    float intery = yend + gradient;
    
    // Handle second endpoint
    xend = roundf(x1);
    float yend2 = y1 + gradient * (xend - x1);
    xgap = fpart(x1 + 0.5);
    int xpxl2 = (int)xend;
    int ypxl2 = (int)yend2;
    
    if (steep) {
        plot(ypxl2, xpxl2, rfpart(yend2) * xgap, color);
        plot(ypxl2 + 1, xpxl2, fpart(yend2) * xgap, color);
    } else {
        plot(xpxl2, ypxl2, rfpart(yend2) * xgap, color);
        plot(xpxl2, ypxl2 + 1, fpart(yend2) * xgap, color);
    }
    
    // Main loop
    if (steep) {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot((int)intery, x, rfpart(intery), color);
            plot((int)intery + 1, x, fpart(intery), color);
            intery += gradient;
        }
    } else {
        for (int x = xpxl1 + 1; x < xpxl2; x++) {
            plot(x, (int)intery, rfpart(intery), color);
            plot(x, (int)intery + 1, fpart(intery), color);
            intery += gradient;
        }
    }
}

// Animation mode: 3D Wireframe Cube with Grid - Tech aesthetic
void DrawTeapot() {
    MPU6050_t mpu_data;
    float angleX = 0, angleY = 0, angleZ = 0;
    const float origin_x = 255.0;  // Center of 70px area (220 + 35)
    const float origin_y = 195.0;  // Center of 70px area (160 + 35)
    
    // Smoothing for rotation
    const float smoothing = 0.2;  // Lower = smoother but more lag
    // Cube vertices with grid subdivisions
    const int8_t cube_verts[][3] = {
        // Main cube corners (0-7)
        {-20, -20, -20}, {20, -20, -20}, {20, 20, -20}, {-20, 20, -20},  // Back face
        {-20, -20, 20}, {20, -20, 20}, {20, 20, 20}, {-20, 20, 20},      // Front face
        // Front face grid points (z=20) (8-10)
        {-10, -20, 20}, {0, -20, 20}, {10, -20, 20},      // 8,9,10: bottom edge subdivisions
        {-20, 0, 20}, {20, 0, 20},                         // 11,12: left/right center
        {-10, 20, 20}, {0, 20, 20}, {10, 20, 20},         // 13,14,15: top edge subdivisions
        // Back face grid points (z=-20) (16-21)
        {-10, -20, -20}, {0, -20, -20}, {10, -20, -20},   // 16,17,18: bottom edge subdivisions
        {-20, 0, -20}, {20, 0, -20},                       // 19,20: left/right center
        {-10, 20, -20}, {0, 20, -20}, {10, 20, -20}       // 21,22,23: top edge subdivisions
    };
    const uint8_t num_verts = 24;
    
    // Edges (cube outline + grid)
    const uint8_t edges[][2] = {
        // Cube edges (12 edges)
        {0,1}, {1,2}, {2,3}, {3,0},    // Back face outline
        {4,5}, {5,6}, {6,7}, {7,4},    // Front face outline
        {0,4}, {1,5}, {2,6}, {3,7},    // Connecting edges
        // Front face grid (horizontal lines)
        {4,8}, {8,9}, {9,10}, {10,5},  // Bottom edge with subdivisions
        {11,12},                        // Middle horizontal
        {7,13}, {13,14}, {14,15}, {15,6}, // Top edge with subdivisions
        // Back face grid (horizontal lines)
        {0,16}, {16,17}, {17,18}, {18,1}, // Bottom edge with subdivisions
        {19,20},                           // Middle horizontal
        {3,21}, {21,22}, {22,23}, {23,2}  // Top edge with subdivisions
    };
    const uint8_t num_edges = 30;
    
    float projected[24][2];
    uint16_t recovery_count = 0;
    
    while(1){
        // Read MPU6050 orientation
        xSemaphoreTake(i2c_xSemaphore, I2C_TIMEOUT_MS);
        MPU6050_Read_All(&mpu_data);
        xSemaphoreGive(i2c_xSemaphore);
        
        // Check if MPU6050 has frozen (angle out of valid range, NaN, or infinity)
        if(isnan(mpu_data.ComplementaryAngleX) || isnan(mpu_data.ComplementaryAngleY) ||
           isinf(mpu_data.ComplementaryAngleX) || isinf(mpu_data.ComplementaryAngleY) ||
           mpu_data.ComplementaryAngleX < -180.0 || mpu_data.ComplementaryAngleX > 180.0 ||
           mpu_data.ComplementaryAngleY < -180.0 || mpu_data.ComplementaryAngleY > 180.0) {
            recovery_count++;
            if(recovery_count > 500) {
                //ESP_LOGI("DrawTeapot", "MPU6050 frozen detected (X:%.2f Y:%.2f), attempting recovery %d/3",
                //        mpu_data.ComplementaryAngleX, mpu_data.ComplementaryAngleY, recovery_count);
                xSemaphoreTake(i2c_xSemaphore, I2C_TIMEOUT_MS);
                //MPU6050_WHO_AM_I();
                //i2c_reset();
                //restart esp
                //esp_restart();
                xSemaphoreGive(i2c_xSemaphore);
                
                angleX = 0;
                angleY = 0;
                
                vTaskDelay(200 / portTICK_PERIOD_MS);  // Allow sensor to fully stabilize
                continue;  // Skip this frame
            } else {
                // Too many recovery attempts, use safe default values
                //ESP_LOGW("DrawTeapot", "MPU6050 recovery failed, using default angles");
                mpu_data.ComplementaryAngleX = 0.0;
                mpu_data.ComplementaryAngleY = 0.0;
            }
        } else {
            // Valid readings, reset recovery counter
            recovery_count = 0;
        }
        
        // Use complementary filter angles (in degrees, convert to radians)
        float targetX = mpu_data.ComplementaryAngleX * 0.0174533;
        float targetY = mpu_data.ComplementaryAngleY * 0.0174533;
        
        // Debug display - show target angles
        //char debug_text[32];
        //sprintf(debug_text, "X:%.2f Y:%.2f", mpu_data.ComplementaryAngleX, mpu_data.ComplementaryAngleY);
        //show_text((uint8_t*)debug_text, &white_color, &bg_color, 160, 45, char_map25, font_pixels25);
        
        // Smooth transition to target angles (exponential smoothing)
        angleX += (targetX - angleX) * smoothing;
        angleY += (targetY - angleY) * smoothing;
        
        // Auto-rotation - smooth and continuous
        angleZ += 0.02;
        if(angleZ > 6.28319) angleZ -= 6.28319;  // Wrap around at 2*PI
        //Auto -rotation x and y (slower)
        angleX += 0.005;
        angleY += 0.003;
        if(angleX > 6.28319) angleX -= 6.28319;
        if(angleY > 6.28319) angleY -= 6.28319;
        
        // Precompute sin/cos for all axes
        float sinX = sinf(angleX), cosX = cosf(angleX);
        float sinY = sinf(angleY), cosY = cosf(angleY);
        float sinZ = sinf(angleZ), cosZ = cosf(angleZ);
        
        // Project vertices with 3-axis rotation
        for(int i = 0; i < num_verts; i++){
            int8_t x = cube_verts[i][0];
            int8_t y = cube_verts[i][1];
            int8_t z = cube_verts[i][2];
            
            // Rotate around X axis
            float y1 = y * cosX - z * sinX;
            float z1 = y * sinX + z * cosX;
            
            // Rotate around Y axis
            float x2 = x * cosY + z1 * sinY;
            //float z2 = -x * sinY + z1 * cosY;
            
            // Rotate around Z axis
            float x3 = x2 * cosZ - y1 * sinZ;
            float y3 = x2 * sinZ + y1 * cosZ;
            
            // Perspective projection
            projected[i][0] = origin_x + x3;
            projected[i][1] = origin_y + y3;
        }
        
        // Clear frame area (70x70 pixels = 4900 pixels, same as bird)
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        
        uint8_t bg_hi = (bg_color.r << 3) | (bg_color.g >> 3);
        uint8_t bg_lo = (bg_color.g << 5) | bg_color.b;
        
        for(int y = 0; y < 70; y++) {
            int row_idx = (160 + y) * 640 + 440; // 440 = 220 * 2
            for(int x = 0; x < 70; x++) {
                int idx = row_idx + x * 2;
                lcd_buffer[idx] = bg_hi;
                lcd_buffer[idx + 1] = bg_lo;
            }
        }
        
        // Draw corner markers for tech aesthetic
        for(int i = 0; i < 3; i++){
            write_buffer(lcd_buffer, 220 + i, 160, dark_cyan_color);
            write_buffer(lcd_buffer, 220, 160 + i, dark_cyan_color);
            write_buffer(lcd_buffer, 289 - i, 160, dark_cyan_color);
            write_buffer(lcd_buffer, 289, 160 + i, dark_cyan_color);
            write_buffer(lcd_buffer, 220 + i, 229, dark_cyan_color);
            write_buffer(lcd_buffer, 220, 229 - i, dark_cyan_color);
            write_buffer(lcd_buffer, 289 - i, 229, dark_cyan_color);
            write_buffer(lcd_buffer, 289, 229 - i, dark_cyan_color);
        }
        
        // Draw edges using Xiaolin Wu's anti-aliased line algorithm
        for(int e = 0; e < num_edges; e++){
            draw_line_aa(projected[edges[e][0]][0], projected[edges[e][0]][1],
                         projected[edges[e][1]][0], projected[edges[e][1]][1], white_color);
        }
        
        xSemaphoreGive(spi_xSemaphore);
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

// Main animation dispatcher - Add new animations here
void DrawAnimation() {
    switch(animation_mode) {
        case ANIM_BIRD:
            DrawBirdAnimation();
            break;
            
        case ANIM_TEAPOT:
            DrawTeapot();
            break;
            
        // Add new animation cases here in the future:
        // case ANIM_YOUR_NEW_MODE:
        //     DrawYourNewAnimation();
        //     break;
        
        default:
            DrawBirdAnimation();  // Fallback to bird animation
            break;
    }
}

void DrawCloud(){
    int Xs = 55;
    int step = 1;
    float alpha, oneminusalpha;
    uint8_t v;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
        
        for(int y = 0; y < 25; y++) {
            int buffer_base_idx = (25 + y) * 640 + Xs * 2;
            int cloud_base_idx = y * 27;
            
            for(int x = 0; x < 27; x++) {
                v = cloudd[cloud_base_idx + x];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((cloud_color.r * alpha) + (oneminusalpha * bg_color.r));
                    last_color.g = (uint8_t)((cloud_color.g * alpha) + (oneminusalpha * bg_color.g));
                    last_color.b = (uint8_t)((cloud_color.b * alpha) + (oneminusalpha * bg_color.b));
                    last_v = v;
                }
                
                int idx = buffer_base_idx + x * 2;
                lcd_buffer[idx] = (last_color.r << 3) | (last_color.g >> 3);
                lcd_buffer[idx + 1] = (last_color.g << 5) | last_color.b;
            }
        }
        
        xSemaphoreGive(spi_xSemaphore);
        if(Xs == 105){
            step = -1;
        } else if(Xs == 55){
            step = 1;
        }
        Xs += step;
        vTaskDelay(60 / portTICK_PERIOD_MS);
    }
}

void display_clock(void * pvParameters) {
    time_t now;
    struct tm timeinfo;
    uint16_t Xs, Ys = 55, Xe, Ye;
    uint8_t v;
    struct clock_char font_c;
    float alpha, oneminusalpha;
    temp_clock[2] = 58;
    temp_clock[5] = 58;
    uint8_t last_clock[8] = {255, 255, 255, 255, 255, 255, 255, 255};
    time(&now);
    localtime_r(&now, &timeinfo);
    int date = timeinfo.tm_mday;
    color_struct last_color;
    uint16_t last_v = 256;
    while(1){
        if (g_force_clock_redraw) {
            memset(last_clock, 255, sizeof(last_clock));
            g_force_clock_redraw = false;
        }
        Xs = 15;
        time(&now);
        localtime_r(&now, &timeinfo);
        temp_clock[0] = timeinfo.tm_hour / 10 + 48;
        temp_clock[1] = timeinfo.tm_hour % 10 + 48;
        temp_clock[3] = timeinfo.tm_min / 10 + 48;
        temp_clock[4] = timeinfo.tm_min % 10 + 48;
        temp_clock[6] = timeinfo.tm_sec / 10 + 48;
        temp_clock[7] = timeinfo.tm_sec % 10 + 48;
        for(int i = 0; i < 8; i++){
            font_c = clock_map[temp_clock[i]];
            Xe = Xs + font_c.w + font_c.left;
            Ye = Ys + font_c.h + font_c.top;
            if(temp_clock[i] == last_clock[i]){
                Xs = Xs + font_c.advance;
                continue;
            }
            last_v = 256;  // Reset color cache for new digit
            xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
            
            uint8_t bg_hi = (bg_color.r << 3) | (bg_color.g >> 3);
            uint8_t bg_lo = (bg_color.g << 5) | bg_color.b;
            
            for(int y = Ys; y < Ye; y++){
                int row_idx = y * 640 + Xs * 2;
                for(int x = Xs; x < Xe; x++){
                    lcd_buffer[row_idx++] = bg_hi;
                    lcd_buffer[row_idx++] = bg_lo;
                }
            }
            
            for(int y = Ys + font_c.top; y < Ye; y++){
                for(int x = Xs + font_c.left; x < Xe; x++){
                    v = clock_pixels[font_c.offset + (x - Xs - font_c.left) + (y - Ys - font_c.top) * font_c.w];
                    if(v != last_v){
                        alpha = (float)v / 255;
                        oneminusalpha = 1 - alpha;
                        last_color.r = (uint8_t)((clock_color.r * alpha) + (oneminusalpha * bg_color.r));
                        last_color.g = (uint8_t)((clock_color.g * alpha) + (oneminusalpha * bg_color.g));
                        last_color.b = (uint8_t)((clock_color.b * alpha) + (oneminusalpha * bg_color.b));
                        last_v = v;
                    }
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
            xSemaphoreGive(spi_xSemaphore);
            Xs = Xs + font_c.advance;
            last_clock[i] = temp_clock[i];
        }
        if(date != timeinfo.tm_mday){
            display_date();
            date = timeinfo.tm_mday;
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

void update_wifi_status_display() {
    uint8_t v;
    uint16_t Xs = 285, Ys = 10, Xe, Ye;
    struct icon_char icon_c;
    float alpha, oneminusalpha;
    wifi_ap_record_t ap_info;
    esp_err_t err;
    uint16_t Xssid;
    color_struct last_color;
    uint16_t last_v = 256;

    err = esp_wifi_sta_get_ap_info(&ap_info);
    if(err == ESP_OK){
        if(ap_info.rssi < -90){
            icon_c = icon_map[1];
            wifi_color = orange_color;
        } else if(ap_info.rssi < -60){
            icon_c = icon_map[2];
            wifi_color = yellow_color;
        } else {
            icon_c = icon_map[3];
            wifi_color = green_color; 
        }
        if(strlen((char*)ap_info.ssid) > 10){
            ap_info.ssid[10] =
            ap_info.ssid[11] = '.';
            ap_info.ssid[12] = '\0';
        }
    } else {
        wifi_color = red_color;
        memset(&ap_info.ssid, ' ', sizeof(ap_info.ssid));
        ap_info.ssid[10] = '\0';
        icon_c = icon_map[0];
    }
    Xssid = 280 - 10;
    for(int i = 0; i < strlen((char*)ap_info.ssid); i++){
        Xssid -= char_map25[ap_info.ssid[i]].advance;
    }
    show_text((uint8_t*)"   ", &wifi_color, &bg_color, Xssid - 18, 15, char_map25, font_pixels25);
    show_text(ap_info.ssid, &wifi_color, &bg_color, Xssid, 15, char_map25, font_pixels25);
    
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    Xe = Xs + 30;
    Ye = Ys + 30;
    
    uint8_t bg_hi = (bg_color.r << 3) | (bg_color.g >> 3);
    uint8_t bg_lo = (bg_color.g << 5) | bg_color.b;
    
    for(int y = Ys; y < Ye; y++){
        int row_idx = y * 640 + Xs * 2;
        for(int x = Xs; x < Xe; x++){
            lcd_buffer[row_idx++] = bg_hi;
            lcd_buffer[row_idx++] = bg_lo;
        }
    }
    
    for(int y = Ys; y < Ye; y++){
        for(int x = Xs; x < Xe; x++){
            v = icon_pixels[icon_c.offset + (x - Xs) + (y - Ys) * icon_c.w];
            if(v != last_v){
                alpha = (float)v / 255;
                oneminusalpha = 1 - alpha;
                last_color.r = (uint8_t)((wifi_color.r * alpha) + (oneminusalpha * bg_color.r));
                last_color.g = (uint8_t)((wifi_color.g * alpha) + (oneminusalpha * bg_color.g));
                last_color.b = (uint8_t)((wifi_color.b * alpha) + (oneminusalpha * bg_color.b));
                last_v = v;
            }
            write_buffer(lcd_buffer, x, y, last_color);
        }
    }
    xSemaphoreGive(spi_xSemaphore);
}

void draw_wifi_status(){
    wifi_ap_record_t ap_info;
    esp_err_t err;
    while(1){
        err = esp_wifi_sta_get_ap_info(&ap_info);
        if(err != ESP_OK){
            esp_wifi_connect();
        }
        update_wifi_status_display();
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}

void update_temp_humi_display(float temperature, float humidity) {
    char buffer_temp[10];
    char buffer_humi[10];
    sprintf(buffer_temp, "%.1f %cC   ", temperature, 176);
    sprintf(buffer_humi, "%.1f %%   ", humidity);
    show_text((uint8_t*)buffer_temp, &white_color, &bg_color, 145, 170, char_map25, font_pixels25);
    show_text((uint8_t*)buffer_humi, &white_color, &bg_color, 105, 200, char_map25, font_pixels25);
}

void print_temperature_and_humidity(){
    show_text((uint8_t*)"Temperature:", &white_color, &bg_color, 20, 170, char_map25, font_pixels25);
    show_text((uint8_t*)"Humidity:", &white_color, &bg_color, 20, 200, char_map25, font_pixels25);
    while(1){
        xSemaphoreTake(i2c_xSemaphore, I2C_TIMEOUT_MS);
        aht30_read(&last_humi, &last_temp);
        xSemaphoreGive(i2c_xSemaphore);
        update_temp_humi_display(last_temp, last_humi);
        vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

void display_date(){
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    if(timeinfo.tm_wday == 0){
        buffer_date[0] = 'C';
        buffer_date[1] = 'N';
    } else {
        buffer_date[0] = 'T';
        buffer_date[1] = timeinfo.tm_wday + 1 + 48;
    }
    buffer_date[5] = timeinfo.tm_mday / 10 + 48;
    buffer_date[6] = timeinfo.tm_mday % 10 + 48;
    timeinfo.tm_mon++;
    buffer_date[10] = timeinfo.tm_mon / 10 + 48;
    buffer_date[11] = timeinfo.tm_mon % 10 + 48;
    timeinfo.tm_year+=1900;
    buffer_date[15] = timeinfo.tm_year / 1000 % 10 + 48;
    buffer_date[16] = timeinfo.tm_year / 100 % 10 + 48;
    buffer_date[17] = timeinfo.tm_year / 10 % 10 + 48;
    buffer_date[18] = timeinfo.tm_year % 10 + 48;
    show_text(buffer_date, &date_color, &bg_color, 60, 120, char_map30, font_pixels30);
}

void redraw_home_static() {
    display_date();
    show_text((uint8_t*)"Temperature:", &white_color, &bg_color, 20, 170, char_map25, font_pixels25);
    show_text((uint8_t*)"Humidity:", &white_color, &bg_color, 20, 200, char_map25, font_pixels25);
    
    g_force_clock_redraw = true;
    update_wifi_status_display();
    update_temp_humi_display(last_temp, last_humi);
}