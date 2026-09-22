#include <stdio.h>
#include <driver/ledc.h>
#include <soc/gpio_num.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <portmacro.h>
#include <esp_adc/adc_oneshot.h>

#define SERVO_CONTROL_PIN GPIO_NUM_7
#define INPUT_ADC_CHANNEL ADC_CHANNEL_4
#define LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0

#define POTENTIOMETER_ANGLE_MAX 300
#define SERVO_PERIOD_US 20000
#define SERVO_MAX_DUTY (1u << 14)
#define SERVO_MIN_PULSE 500
#define SERVO_MAX_PULSE 2500

static adc_oneshot_unit_handle_t adc_handle;

void init_adc(void);
_Noreturn void read_adc(void *pvParameters);
_Noreturn void servo_control_task(void *pvParameters);
void init_pwm(void);
void servo_set_duty(uint32_t pulse_width_us);
void servo_set_angle(float angle_deg);
float convert_adc_into_angle(int adc_value);

volatile int potentiometer_raw_value = 0;

void app_main(void)
{
    init_adc();
    init_pwm();
    xTaskCreate(read_adc, "read_adc", 4096, NULL, 10, NULL);
    xTaskCreate(servo_control_task, "servo_control_task", 4096, NULL, 10, NULL);
}

_Noreturn void read_adc(void *pvParameters) {
    int adc_raw = 0;
    while(1) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, INPUT_ADC_CHANNEL, &adc_raw));
        potentiometer_raw_value = adc_raw;
        int calculated_voltage_mV = adc_raw * 3300 / 4095;
        printf("ADC raw: %d, calculated voltage: %d mV\n", adc_raw, calculated_voltage_mV);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

_Noreturn void servo_control_task(void *pvParameters) {
    static float last_set_angle = 0;
    while(1) {
        float angle = convert_adc_into_angle(potentiometer_raw_value);
        if (angle != last_set_angle) {
            servo_set_angle(angle);
            last_set_angle = angle;
            vTaskDelay(pdMS_TO_TICKS(400i));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



void servo_set_duty(uint32_t pulse_width_us) {
    uint32_t duty_count = (uint32_t)(((float)pulse_width_us / SERVO_PERIOD_US) * SERVO_MAX_DUTY);
    ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL, duty_count);
    ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL);
}

void servo_set_angle(float angle_deg) {
    if (angle_deg > 180) {
        angle_deg = 180;
    }
    int pulse_width_us = SERVO_MIN_PULSE + (angle_deg / 180.0f) * (SERVO_MAX_PULSE - SERVO_MIN_PULSE);
    servo_set_duty(pulse_width_us);
}

float convert_adc_into_angle(int adc_value) {
    float potentiometer_angle = ((float) adc_value / 4095) * POTENTIOMETER_ANGLE_MAX;

    return potentiometer_angle;
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

void init_pwm(void) {
    ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_SPEED_MODE,
            .timer_num = LEDC_TIMER ,
            .duty_resolution = LEDC_TIMER_14_BIT,
            .freq_hz = 50,
            .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_config);
    ledc_channel_config_t channel_config = {
            .gpio_num = SERVO_CONTROL_PIN,
            .speed_mode = LEDC_SPEED_MODE,
            .channel = LEDC_CHANNEL,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
    };
    ledc_channel_config(&channel_config);
}
