#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <soc/gpio_num.h>
#include <esp_log.h>
#include <driver/gpio.h>
#include <hal/gpio_types.h>
#include "driver/pulse_cnt.h"
#include <esp_timer.h>
#include <driver/ledc.h>

#define COUNTER_MAX 100
#define COUNTER_MIN -100
#define SERVO_PERIOD_US 20000
#define SERVO_MAX_DUTY (1u << 14)
#define SERVO_MIN_PULSE 500
#define SERVO_MAX_PULSE 2500
#define DEBOUNCE_US 500000LL
#define SERVO_CONTROL_PIN GPIO_NUM_5
#define LEDC_SPEED_MODE LEDC_LOW_SPEED_MODE
#define LEDC_TIMER LEDC_TIMER_0
#define LEDC_CHANNEL LEDC_CHANNEL_0

static const char *TAG = "safebox";
static volatile int64_t last_isr_time_us = 0;

constexpr gpio_num_t sw_pin = GPIO_NUM_17;
constexpr gpio_num_t dt_pin = GPIO_NUM_16;
constexpr gpio_num_t clk_pin = GPIO_NUM_15;

static const gpio_num_t led_pins[4] = {GPIO_NUM_9, GPIO_NUM_10, GPIO_NUM_11, GPIO_NUM_12};
constexpr int led_pin_mask = (1ULL << GPIO_NUM_9) | (1ULL << GPIO_NUM_10) | (1ULL << GPIO_NUM_11) | (1ULL << GPIO_NUM_12);

int secret_code[4] = {4, 2, 0, 3};

static volatile int dial_value = 0;
static volatile int current_digit_decoding = 0;
static pcnt_unit_handle_t pcnt_unit_handle = NULL;
static bool pcnt_initialized = false;
static TaskHandle_t dial_task_handle = NULL;
static TaskHandle_t led_task_handle = NULL;

enum SAFE_BOX_STATE {
    UNKNOWN,
    SAFE_BOX_SECRET_SETUP,
    SAFE_BOX_DIAL_READING,
    SAFE_BOX_OPEN,
    SAFE_BOX_ERROR
};
static enum SAFE_BOX_STATE safe_box_state = SAFE_BOX_DIAL_READING;

void init_pcnt(void);
void set_led_state(int led_index);
void init_led(void);
void init_clk_button(void);
void init_pwm(void);
void servo_set_duty(uint32_t pulse_width_us);
void servo_set_angle(float angle_deg);
_Noreturn void set_led_flashing_pattern(void *pvParameters);
_Noreturn void dial_task(void *pvParameters);

static void log_dial_state(const int digits_entered[], int current_digit, int current_value) {
    char display[4];
    for (int i = 0; i < 4; i++) {
        if (i < current_digit) {
            display[i] = '0' + digits_entered[i];
        } else if (i == current_digit) {
            display[i] = '0' + current_value;
        } else {
            display[i] = '*';
        }
    }
    ESP_LOGI(TAG, "[ %c %c %c %c ]", display[0], display[1], display[2], display[3]);
}

static void IRAM_ATTR button_isr_handler(void *arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_isr_time_us < DEBOUNCE_US || safe_box_state == SAFE_BOX_DIAL_READING) return;

    ESP_DRAM_LOGI("TAG", "Button isr handler");
    last_isr_time_us = now;
    current_digit_decoding = 0;
    dial_value = 0;
    safe_box_state = SAFE_BOX_DIAL_READING;
}

void app_main(void) {
    init_led();
    init_clk_button();
    init_pwm();
    gpio_install_isr_service(0);
    gpio_isr_handler_add(sw_pin, button_isr_handler, NULL);

    enum SAFE_BOX_STATE last_app_state = UNKNOWN;
    while (1) {
        if (last_app_state != safe_box_state) {
            switch (safe_box_state) {
                case SAFE_BOX_SECRET_SETUP:
                    printf("Please enter the secret_code: ");
                    scanf("%1d%1d%1d%1d", &secret_code[0], &secret_code[1], &secret_code[2], &secret_code[3]);
                    safe_box_state = SAFE_BOX_DIAL_READING;
                    break;
                case SAFE_BOX_DIAL_READING:
                    servo_set_angle(90);
                    if (led_task_handle != NULL) {
                        vTaskDelete(led_task_handle);
                        led_task_handle = NULL;
                    }
                    if (dial_task_handle != NULL) {
                        vTaskDelete(dial_task_handle);
                        dial_task_handle = NULL;
                    }
                    if (!pcnt_initialized) {
                        init_pcnt();
                        pcnt_initialized = true;
                    }
                    current_digit_decoding = 0;
                    xTaskCreate(dial_task, "dial_task", 4096, NULL, 10, &dial_task_handle);
                    break;
                case SAFE_BOX_OPEN:
                    servo_set_angle(0);
                    goto spawn_led_task;
                case SAFE_BOX_ERROR:
                    servo_set_angle(90);
                spawn_led_task:
                    if (dial_task_handle != NULL) {
                        vTaskDelete(dial_task_handle);
                        dial_task_handle = NULL;
                    }
                    xTaskCreate(set_led_flashing_pattern, "set_led_flashing_pattern", 4096, (void *) safe_box_state, 10, &led_task_handle);
                    break;
                case UNKNOWN:
                    break;
            }
            last_app_state = safe_box_state;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

_Noreturn void dial_task(void *pvParameters) {
    int digits_entered[4];
    bool is_digit_decoding_finished = false;

    int last_count = 0;
    int last_delta = 0;
    int last_digit = 0;
    while (1) {
        if (current_digit_decoding != last_digit) {
            set_led_state(current_digit_decoding);
            last_digit = current_digit_decoding;
        }
        int raw = 0;
        pcnt_unit_get_count(pcnt_unit_handle, &raw);
        int delta = raw - last_count;
        if (delta != 0) {
            is_digit_decoding_finished = (last_delta > 0 && delta < 0) || (last_delta < 0 && delta > 0);
            last_delta = delta;
            int steps_count = delta / 2;

            if (is_digit_decoding_finished) {
                digits_entered[current_digit_decoding] = dial_value;
                current_digit_decoding++;
                is_digit_decoding_finished = false;
            }
            if (current_digit_decoding == 4) {
                bool is_secret_code_correct = true;
                for (int i = 0; i < 4; i++) {
                    if (digits_entered[i] != secret_code[i]) {
                        is_secret_code_correct = false;
                        break;
                    }
                }
                if (is_secret_code_correct) {
                    safe_box_state = SAFE_BOX_OPEN;
                } else {
                    safe_box_state = SAFE_BOX_ERROR;
                }
                vTaskSuspend(NULL);
            }
            dial_value = (dial_value + steps_count + 10) % 10;
            log_dial_state(digits_entered, current_digit_decoding, dial_value);
            last_count = raw;
        }
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}

void set_led_state(int led_index) {
    int led_count = sizeof(led_pins) / sizeof(led_pins[0]);
    for (int i = 0; i < led_count; i++) {
        gpio_set_level(led_pins[i], i < led_index ? 1 : 0);
    }
}

_Noreturn void set_led_flashing_pattern(void *pvParameters) {
    int led_count = sizeof(led_pins) / sizeof(led_pins[0]);
    while (1) {
        switch (safe_box_state) {
            case UNKNOWN:
            case SAFE_BOX_SECRET_SETUP:
            case SAFE_BOX_DIAL_READING:
                break;
            case SAFE_BOX_OPEN:
                for (int i = 0; i < led_count; i++) {
                    gpio_set_level(led_pins[i], 1);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    gpio_set_level(led_pins[i], 0);
                }
                for (int i = led_count - 2; i > 0; i--) {
                    gpio_set_level(led_pins[i], 1);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    gpio_set_level(led_pins[i], 0);
                }
                break;
            case SAFE_BOX_ERROR:
                for (int i = 0; i < led_count; i++) {
                    gpio_set_level(led_pins[i], 1);
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                for (int i = 0; i < led_count; i++) {
                    gpio_set_level(led_pins[i], 0);
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                break;
        }
    }
}

void init_pcnt(void) {
    pcnt_unit_config_t unit_config = {
            .high_limit = COUNTER_MAX,
            .low_limit = COUNTER_MIN
    };

    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit_handle));

    pcnt_glitch_filter_config_t filter_config = {
            .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit_handle, &filter_config));

    pcnt_chan_config_t chan_config_a = {
            .edge_gpio_num = clk_pin,
            .level_gpio_num = dt_pin,
    };

    pcnt_channel_handle_t chan_handle_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_handle, &chan_config_a, &chan_handle_a));

    pcnt_chan_config_t chan_config_b = {
            .edge_gpio_num = dt_pin,
            .level_gpio_num = clk_pin,
    };
    pcnt_channel_handle_t chan_handle_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit_handle, &chan_config_b, &chan_handle_b));
    /*
     * init channel A
     * */
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_handle_a, PCNT_CHANNEL_EDGE_ACTION_INCREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_handle_a, PCNT_CHANNEL_LEVEL_ACTION_HOLD,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    /*
     * init channel B
     * */
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_handle_b, PCNT_CHANNEL_EDGE_ACTION_DECREASE,
                                                 PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_handle_b, PCNT_CHANNEL_LEVEL_ACTION_HOLD,
                                                  PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit_handle));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit_handle));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit_handle));
}

void init_led(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = led_pin_mask;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

void init_clk_button(void) {
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pin_bit_mask = (1ULL << sw_pin);
    gpio_config(&io_conf);
}

void init_pwm(void) {
    ledc_timer_config_t timer_config = {
            .speed_mode = LEDC_SPEED_MODE,
            .timer_num = LEDC_TIMER ,
            .duty_resolution = LEDC_TIMER_14_BIT,
            .freq_hz = 50,
            .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));
    ledc_channel_config_t channel_config = {
            .gpio_num = SERVO_CONTROL_PIN,
            .speed_mode = LEDC_SPEED_MODE,
            .channel = LEDC_CHANNEL,
            .timer_sel = LEDC_TIMER,
            .duty = 0,
            .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

void servo_set_duty(uint32_t pulse_width_us) {
    uint32_t duty_count = (uint32_t)(((float)pulse_width_us / SERVO_PERIOD_US) * SERVO_MAX_DUTY);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_SPEED_MODE, LEDC_CHANNEL, duty_count));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_SPEED_MODE, LEDC_CHANNEL));
}

void servo_set_angle(float angle_deg) {
    if (angle_deg > 180) angle_deg = 180;
    if (angle_deg < 0)   angle_deg = 0;
    int pulse_width_us = SERVO_MIN_PULSE + (angle_deg / 180.0f) * (SERVO_MAX_PULSE - SERVO_MIN_PULSE);
    servo_set_duty(pulse_width_us);
}