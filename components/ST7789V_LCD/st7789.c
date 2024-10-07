#include "st7789.h"
#include "define.h"
typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes; // No of data in data; bit 7 = delay after set; 0xFF = end
                       // of cmds.
} lcd_init_cmd_t;

lcd_init_cmd_t st7789v_init_command[] = {
    // Sleep out
    {0x11, {0x00}, 0},
    // Normal Display Mode On
    {0x13, {0x00}, 0},
    // Gamma Set
    {0x51, {0x01}, 1},
    // Memory Data Access Control
    {0x36, {0x60}, 1},
    // Idle Mode Off
    {0x38, {0x00}, 0},
    // Vertical Scrolling Definition
    {0x33, {0x00, 0x00, 0x01, 0x40, 0x00, 0x00}, 6},
    // Interface Pixel Format
    {0x3A, {0x63}, 1},
    // Write CTRL Display
    //{0x53, {0x24}, 1},
    // Write Display Brightness
    {0x51, {0x7F}, 1},
    // RAM Control
    //{0xB0, {0x00, 0xE0}, 2},
    // Frame Rate Control in Normal Mode
    {0x06, {0x05}, 1},
    // Display Inversion Off
    {0x21, {0x00}, 0},
    // Display On
    {0x29, {0x00}, 0x08},
};

void IRAM_ATTR lcd_spi_pre_transfer_callback(spi_transaction_t *t) {
    int dc = (int)t->user;
    gpio_set_level(PIN_NUM_DC, dc);
}

void IRAM_ATTR lcd_cmd(spi_device_handle_t spi, const uint8_t cmd, bool keep_cs_active) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t)); // Zero out the transaction
    t.length = 8;             // Command is 8 bits
    t.tx_buffer = &cmd;       // The data is the cmd itself
    t.user = (void *)0;       // D/C needs to be set to 0
    if (keep_cs_active) {
        t.flags = SPI_TRANS_CS_KEEP_ACTIVE; // Keep CS active after data transfer
    }
    ret = spi_device_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);                      // Should have had no issues.
}

void IRAM_ATTR lcd_data(spi_device_handle_t spi, const uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) {
        return; // no need to send anything
    }
    memset(&t, 0, sizeof(t));                   // Zero out the transaction
    t.length = len * 8;   
    t.flags = SPI_TRANS_CS_KEEP_ACTIVE; // Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;                         // Data
    t.user = (void *)1;                         // D/C needs to be set to 1
    ret = spi_device_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);                      // Should have had no issues.
}

static spi_transaction_t trans[6];
void IRAM_ATTR lcd_print_data(spi_device_handle_t spi, uint16_t Xs, uint16_t Xe,
                     uint16_t Ys, uint16_t Ye, const uint8_t *data, int len) {
    esp_err_t ret; 
    // Column Address Set;
    trans[0].tx_data[0] = 0x2A;
    trans[1].tx_data[0] = Xs >> 8;
    trans[1].tx_data[1] = Xs;
    trans[1].tx_data[2] = Xe >> 8;
    trans[1].tx_data[3] = Xe;
    // Row Address Set
    trans[2].tx_data[0] = 0x2B;
    trans[3].tx_data[0] = Ys >> 8;
    trans[3].tx_data[1] = Ys;
    trans[3].tx_data[2] = Ye >> 8;
    trans[3].tx_data[3] = Ye;
    // Data 
    trans[5].tx_buffer = (uint8_t*)data;   
    trans[5].length = len * 8; 
    for (int i = 0; i < 6; i++) {
        ret = spi_device_queue_trans(spi, &trans[i], portMAX_DELAY);
        assert(ret == ESP_OK);
    }
    spi_transaction_t *rtrans;
    for (int i = 0; i < 6; i++) {
        ret = spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
        assert(ret == ESP_OK);
    }
}

void IRAM_ATTR show_text(spi_device_handle_t spi, const uint8_t *str,
                    color_struct *font_color, color_struct *bg_color, uint16_t Xs, uint16_t Ys,
                    const struct font_char char_map[], const uint8_t font_pixels[]){
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);  
    uint8_t v;
    struct font_char font_c;
    color_struct last_color;
    uint16_t last_v = 256;
    uint16_t Xe, Ye;
    float alpha, oneminusalpha;
    while(*str != '\0') {
        font_c = char_map[*str];
        Xe = Xs + font_c.advance;
        Ye = Ys + font_c.h + font_c.top;
        for(int y = Ys; y < char_map[0].h + Ys; y++){
            for(int x = Xs; x < Xe; x++){
                write_buffer(lcd_buffer, x, y, *bg_color);
            }
        }
        for(int y = Ys + font_c.top; y < Ye; y++){
            for(int x = Xs + font_c.left; x < Xe; x++){
                v = font_pixels[font_c.offset + (x - Xs - font_c.left) + (y - Ys - font_c.top) * font_c.w];
                if(v != last_v){
                    alpha = (float)v / 255;
                    oneminusalpha = 1 - alpha;
                    last_color.r = (uint8_t)((font_color->r * alpha) + (oneminusalpha * bg_color->r));
                    last_color.g = (uint8_t)((font_color->g * alpha) + (oneminusalpha * bg_color->g));
                    last_color.b = (uint8_t)((font_color->b * alpha) + (oneminusalpha * bg_color->b));
                    last_v = v;
                    write_buffer(lcd_buffer, x, y, last_color);
                } else {
                    write_buffer(lcd_buffer, x, y, last_color);
                }
            }
        }
        Xs = Xs + font_c.advance;
        str++;
    };
    xSemaphoreGive(spi_xSemaphore);
}

void spi2_lcd_init(spi_device_handle_t *spi) {
    spi_bus_config_t spiBusCfg = {.mosi_io_num = PIN_NUM_MOSI,
                                  .miso_io_num = -1,
                                  .sclk_io_num = PIN_NUM_CLK,
                                  .quadwp_io_num = -1,
                                  .quadhd_io_num = -1,
                                  .max_transfer_sz = 28800};
    spi_device_interface_config_t devcfg = {
        .mode = 0,
        .clock_speed_hz = SPI_MASTER_FREQ_40M,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 70,
        .flags = SPI_DEVICE_HALFDUPLEX | SPI_DEVICE_3WIRE,
        .pre_cb = lcd_spi_pre_transfer_callback};
    spi_bus_initialize(SPI2_HOST, &spiBusCfg, SPI_DMA_CH_AUTO);
    spi_bus_add_device(SPI2_HOST, &devcfg, spi);

    gpio_set_level(PIN_NUM_RS, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(PIN_NUM_RS, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    spi_device_acquire_bus(*spi, portMAX_DELAY);
    for (int cmd = 0;
            cmd < sizeof(st7789v_init_command) / sizeof(st7789v_init_command[0]);
            cmd++) {
        lcd_cmd(*spi, st7789v_init_command[cmd].cmd, true);
        lcd_data(*spi, st7789v_init_command[cmd].data,
                 st7789v_init_command[cmd].databytes);
        if (st7789v_init_command[cmd].databytes != 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
    }
    printf("LCD ID: %" PRIu32 "\n", lcd_get_id(*spi));
}

void IRAM_ATTR fill_display(spi_device_handle_t spi, color_struct *bg_color) {
    xSemaphoreTake(spi_xSemaphore, portMAX_DELAY);
    for (int y = 0; y < 240; y++) {
        for(int x = 0; x < 320; x++){
            write_buffer(lcd_buffer, x, y, *bg_color);
        }
    }
    xSemaphoreGive(spi_xSemaphore);
}

uint32_t lcd_get_id(spi_device_handle_t spi) {
    for(int i = 0; i < 6; i++){
        memset(&trans[i], 0, sizeof(spi_transaction_t));
    }
    trans[0].flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE;
    trans[1].flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE;
    trans[2].flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE;
    trans[3].flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE;
    trans[4].flags = SPI_TRANS_USE_TXDATA | SPI_TRANS_CS_KEEP_ACTIVE;
    trans[5].flags = SPI_TRANS_CS_KEEP_ACTIVE;

    trans[0].length = 8;
    trans[0].user = (void *)0;
    trans[1].length = 32;
    trans[1].user = (void *)1;
    trans[2].length = 8;
    trans[2].user = (void *)0;
    trans[3].length = 32;
    trans[3].user = (void *)1;
    trans[4].length = 8;
    trans[4].user = (void *)0;
    trans[4].tx_data[0] = 0x2c;
    trans[5].user = (void *)1;
    // get_id cmd
    lcd_cmd(spi, 0x04, true);

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.rxlength = 8 * 3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *)1;
    t.rx_buffer = NULL;

    esp_err_t ret = spi_device_polling_transmit(spi, &t);
    assert(ret == ESP_OK);

    return t.rx_data[0];
}
