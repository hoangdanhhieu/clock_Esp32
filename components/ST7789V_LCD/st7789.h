#include "hal/spi_types.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "stdint.h"
#include "string.h"
#include <stdbool.h>
#include "font.h"
#include "color.h"
extern SemaphoreHandle_t spi_xSemaphore;
extern uint8_t lcd_buffer[115200];
uint32_t lcd_get_id(spi_device_handle_t spi);
void spi2_lcd_init(spi_device_handle_t *spi);
void lcd_print_data(spi_device_handle_t spi, uint16_t Xs, uint16_t Xe, uint16_t Ys, uint16_t Ye, const uint8_t *data, int len);
void lcd_data(spi_device_handle_t spi, const uint8_t *data, int len);
void fill_display(spi_device_handle_t spi, color_struct *bg_color);
void lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active);
void show_text(spi_device_handle_t spi, const uint8_t *str,
                    color_struct *font_color, color_struct *bg_color, uint16_t Xs, uint16_t Ys,
                    const struct font_char char_map[], const uint8_t font_pixels[]);