#include "hal/adc_types.h"
#include "esp_adc/adc_oneshot.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "freertos/task.h"

adc_oneshot_unit_handle_t adc1_handle;

void init_adc_temt6000(){
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_6, &config));
}

int read_temt6000_value(){
    int s = 0;
    int adc_raw;
    for(int i = 0; i < 20; i++){
        adc_oneshot_read(adc1_handle, ADC_CHANNEL_6, &adc_raw);
        s+=adc_raw;
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
    return s/20;
}