#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"

#define INPUT_ADC_CHANNEL ADC_CHANNEL_3

static adc_oneshot_unit_handle_t adc_handle;
static adc_cali_handle_t cali_handle;

void init_adc(void);
void calibrate_adc(void);
_Noreturn void read_adc(void *pvParameters);

void app_main(void)
{
    init_adc();
    calibrate_adc();
    xTaskCreate(&read_adc, "adc_reading_task", 2048, NULL, 5, NULL);
}

_Noreturn void read_adc(void *pvParameters) {
    int adc_raw = 0;
    int calibrated_voltage_mV = 0;
    while(1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, INPUT_ADC_CHANNEL, &adc_raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, adc_raw, &calibrated_voltage_mV));
        int calculated_voltage_mV = adc_raw * 3300 / 4095;
        float pct_diff = (calibrated_voltage_mV != 0)
                         ? (float)(calculated_voltage_mV - calibrated_voltage_mV) / calibrated_voltage_mV * 100.0f
                         : 0.0f;
        printf("ADC raw: %d, calibrated voltage: %d mV, calculated voltage: %d mV, diff: %.02f%%\n", adc_raw, calibrated_voltage_mV, calculated_voltage_mV, pct_diff);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void init_adc() {
    adc_oneshot_unit_init_cfg_t oneshot_init_config = {
            .unit_id = ADC_UNIT_1
    };
    adc_oneshot_new_unit(&oneshot_init_config, &adc_handle);

    adc_oneshot_chan_cfg_t channel_config = {
      .atten = ADC_ATTEN_DB_12,
      .bitwidth = ADC_BITWIDTH_DEFAULT
    };

    adc_oneshot_config_channel(adc_handle, INPUT_ADC_CHANNEL, &channel_config);
}

void calibrate_adc() {
    adc_cali_curve_fitting_config_t curve_fitting_config = {
            .unit_id = ADC_UNIT_1,
            .chan = INPUT_ADC_CHANNEL,
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_DEFAULT
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&curve_fitting_config, &cali_handle));
}