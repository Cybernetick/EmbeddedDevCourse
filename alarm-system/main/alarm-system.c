#include <hal/gpio_types.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"

#define LED_PIN GPIO_NUM_3
#define BUTTON_PIN GPIO_NUM_16
#define ALARM_ADC_CHANNEL ADC_CHANNEL_3   // GPIO4 = ADC1_CH3 on ESP32-S3
#define ALARM_WARNING_PING_DELAY_MS 200
#define LIGHT_ALARM_THRESHOLD 2500        // 0-4095 (12-bit), tune to your photoresistor

static const char *TAG = "BLINKER";
static const char *TAG_LIGHT = "LIGHT_SENSOR";

static volatile bool alarm_charged = false;
static volatile bool light_alarm = false;

static adc_oneshot_unit_handle_t adc1_handle;

static void IRAM_ATTR button_isr_handler(void *arg) {
    alarm_charged = !alarm_charged;
    if (!alarm_charged) {
        light_alarm = false;
    }
}

void init_pins() {
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);

    gpio_reset_pin(BUTTON_PIN);
    gpio_set_direction(BUTTON_PIN, GPIO_MODE_INPUT);
    gpio_set_intr_type(BUTTON_PIN, GPIO_INTR_NEGEDGE);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler, NULL);
}

void init_adc() {
    adc_oneshot_unit_init_cfg_t init_cfg = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&init_cfg, &adc1_handle);

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_12,
        .atten = ADC_ATTEN_DB_12,  // 0-3.3V range
    };
    adc_oneshot_config_channel(adc1_handle, ALARM_ADC_CHANNEL, &chan_cfg);
}

void read_light_sensor(void *pvParameters) {
    int adc_raw = 0;
    while (1) {
        adc_oneshot_read(adc1_handle, ALARM_ADC_CHANNEL, &adc_raw);
        bool triggered = adc_raw < LIGHT_ALARM_THRESHOLD;
        ESP_LOGI(TAG_LIGHT, "raw=%d  light_alarm -> %d", adc_raw, light_alarm);
        if (triggered) {
            light_alarm = triggered;
        }
        vTaskDelay(200 / portTICK_PERIOD_MS);
    }
}

void blink_led(void *pvParameters) {
    int led_level = 0;
    while (1) {
        bool alert = alarm_charged && light_alarm;
        if (alert) {
            led_level = !led_level;
            gpio_set_level(LED_PIN, led_level);
            vTaskDelay(ALARM_WARNING_PING_DELAY_MS / portTICK_PERIOD_MS);
        } else {
            gpio_set_level(LED_PIN, 0);
            led_level = 0;
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
    }
}

void app_main(void)
{
    gpio_install_isr_service(0);
    init_pins();
    init_adc();
    xTaskCreate(&blink_led, "blinker_task", 2048, NULL, 10, NULL);
    xTaskCreate(&read_light_sensor, "light_sensor_task", 2048, NULL, 5, NULL);
}
