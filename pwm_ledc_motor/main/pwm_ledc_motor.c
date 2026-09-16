#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/ledc.h>
#include <hal/ledc_types.h>
#include <soc/clk_tree_defs.h>
#include "driver/gpio.h"
#include <esp_adc/adc_oneshot.h>
#include <stdint-gcc.h>
#include <esp_log.h>
#include <inttypes.h>
#include <stdint.h>

#define LED_GPIO GPIO_NUM_3
#define LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0

#define MOTOR_GPIO GPIO_NUM_9
#define MOTOR_CHANNEL LEDC_CHANNEL_1

#define ADC_CHANNEL ADC_CHANNEL_4

static adc_oneshot_unit_handle_t adc_handle;

static volatile int adc_raw = 0;

void init_ledc_pwm(void);
void init_adc_reading(void);

_Noreturn void adc_reading_task(void *pvParameters);
_Noreturn void led_control_task(void *pvParameters);
_Noreturn void motor_control_task(void *pvParameters);

void app_main(void) {
    init_ledc_pwm();
    init_adc_reading();
    xTaskCreate(adc_reading_task, "adc_reading_task", 4096, NULL, 10, NULL);
    xTaskCreate(led_control_task, "led_control_task", 4096, NULL, 10, NULL);
    xTaskCreate(motor_control_task, "motor_control_task", 4096, NULL, 10, NULL);
}

_Noreturn void adc_reading_task(void *pvParameters) {
    int adc_reading;
    while (1) {
        adc_oneshot_read(adc_handle, ADC_CHANNEL, &adc_reading);
        adc_raw = adc_reading;
        ESP_LOGI("adc_reading_task", "adc_reading: %d", adc_reading);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

_Noreturn void led_control_task(void *pvParameters) {
    while (1) {
        int duty = 0;
        if (adc_raw > 0) {
            duty = ((float) adc_raw / 4096.0f) * 1024;
        }
        ESP_LOGI("led_control_task", "duty: %d", duty);
        ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

_Noreturn void motor_control_task(void *pvParameters) {
    while (1) {
        int duty = 0;
        if (adc_raw > 0) {
            duty = ((float) adc_raw / 4096.0f) * 1024;
        }
        ESP_LOGI("motor_control_task", "duty: %d", duty);
        ledc_set_duty(LEDC_SPEED_MODE, MOTOR_CHANNEL, duty);
        ledc_update_duty(LEDC_SPEED_MODE, MOTOR_CHANNEL);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

void init_ledc_pwm(void) {
    ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_SPEED_MODE,
            .timer_num = LEDC_TIMER ,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .freq_hz = 25000,
            .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_config);
    ledc_channel_config_t channel_config = {
            .gpio_num = LED_GPIO,
            .speed_mode = LEDC_SPEED_MODE,
            .channel = LEDC_CHANNEL,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
    };
    ledc_channel_config_t  motor_channel_config = {
            .gpio_num = MOTOR_GPIO,
            .speed_mode = LEDC_SPEED_MODE,
            .channel = MOTOR_CHANNEL,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
    };
    ledc_channel_config(&channel_config);
    ledc_channel_config(&motor_channel_config);
}

void init_adc_reading(void) {
    adc_oneshot_unit_init_cfg_t init_cfg = {
            .unit_id = ADC_UNIT_1
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg, &adc_handle));

    adc_oneshot_chan_cfg_t channel_config = {
            .atten = ADC_ATTEN_DB_12,
            .bitwidth = ADC_BITWIDTH_12
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &channel_config));
}

