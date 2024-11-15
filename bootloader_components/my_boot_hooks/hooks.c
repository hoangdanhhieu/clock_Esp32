#include "hal/gpio_hal.h"
#include "esp_log.h"
/* Function used to tell the linker to include this file
 * with all its symbols.
 */
void bootloader_hooks_include(void){
}


void bootloader_before_init(void) {
    /* Keep in my mind that a lot of functions cannot be called from here
     * as system initialization has not been performed yet, including
     * BSS, SPI flash, or memory protection. */
    gpio_ll_output_enable(&GPIO, GPIO_NUM_5);
    gpio_ll_set_level(&GPIO, GPIO_NUM_5, 0);
    ESP_LOGI("HOOK", "This hook is called BEFORE bootloader initialization");
}

void bootloader_after_init(void) {
    ESP_LOGI("HOOK", "This hook is called AFTER bootloader initialization");
}
