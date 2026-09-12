#include <stdio.h>
#include <hal/gpio_types.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

static const char *TAG_LIGHT = "LIGHT_SENSOR";

#define LDR_ADC_UNIT ADC_UNIT_1
#define LDR_ADC_CHANNEL ADC_CHANNEL_4
#define LDR_ADC_BITWIDTH ADC_BITWIDTH_12
#define LDR_ADC_ATTEN ADC_ATTEN_DB_12
#define BUFFER_SIZE 8
#define HYSTERESIS_THRESHOLD_LOW 450
#define HYSTERESIS_THRESHOLD_HIGH 750

constexpr gpio_num_t led_signal_pin = GPIO_NUM_3;

static adc_oneshot_unit_handle_t adc1_handle;

static int ldr_buffer[BUFFER_SIZE] = {};
static int buffer_write_index = 0;
static int sma_running_sum = 0;
static volatile int sma = 0;


void setup_adc(void);
void setup_led(void);
_Noreturn void read_light_sensor(void *pvParameters);
_Noreturn void led_control_task(void *pvParameters);

enum LIGHT_STATE {
    ON,
    OFF
};

static enum LIGHT_STATE light_state = OFF;

void app_main(void)
{
    setup_led();
    setup_adc();
    xTaskCreate(read_light_sensor, "read_light_sensor", 4096, NULL, 10, NULL);
    xTaskCreate(led_control_task, "led_control_task", 4096, NULL, 10, NULL);
}

void setup_led() {
    gpio_reset_pin(led_signal_pin);
    gpio_set_direction(led_signal_pin, GPIO_MODE_OUTPUT);
}

void setup_adc(){
    adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = LDR_ADC_UNIT,
    };
    adc_oneshot_new_unit(&init_cfg, &adc1_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
            .bitwidth = LDR_ADC_BITWIDTH,
            .atten = LDR_ADC_ATTEN,
    };
    adc_oneshot_config_channel(adc1_handle, LDR_ADC_CHANNEL, &chan_cfg);
}

_Noreturn void read_light_sensor(void *pvParameters) {
    int adc_raw = 0;
    while (1) {
        adc_oneshot_read(adc1_handle, LDR_ADC_CHANNEL, &adc_raw);
        int current_buffer_element = ldr_buffer[buffer_write_index];
        sma_running_sum -= current_buffer_element;
        sma_running_sum += adc_raw;
        sma = sma_running_sum / BUFFER_SIZE;
        ldr_buffer[buffer_write_index] = adc_raw;
        buffer_write_index = (buffer_write_index + 1) % BUFFER_SIZE;
        ESP_LOGI(TAG_LIGHT, "raw=%d, sma=%d ", adc_raw, sma);
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

_Noreturn void led_control_task(void *pvParameters) {
    while (1) {
        if (sma < HYSTERESIS_THRESHOLD_LOW && light_state == OFF) {
            gpio_set_level(led_signal_pin, 1);
            light_state = ON;
        } else if (sma > HYSTERESIS_THRESHOLD_HIGH && light_state == ON) {
            gpio_set_level(led_signal_pin, 0);
            light_state = OFF;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}