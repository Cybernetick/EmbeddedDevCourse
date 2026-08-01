#include <stdio.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "blinker";
static SemaphoreHandle_t xSemaphore = NULL;

constexpr gpio_num_t LED_PIN_RED = GPIO_NUM_6;
constexpr gpio_num_t PIN_BUTTON = GPIO_NUM_0;
constexpr uint32_t BLINK_DELAY_uS = 500000;

constexpr int led_pin_mask = (1ULL << LED_PIN_RED);

struct LED_STATE {
    gpio_num_t pin;
    bool state;
    uint32_t last_change_microseconds;
};

enum Pattern {
    BLINK,
    CONSTANT_ON,
    CONSTANT_OFF,
};

volatile enum Pattern pattern = BLINK;

static void IRAM_ATTR button_isr_handler(void *arg) {
    pattern = ((enum Pattern)(pattern + 1) % 3);
    xSemaphoreGiveFromISR(xSemaphore, NULL);
}

void init_led(void);
void init_interrupts(void);
void blink(struct LED_STATE *state);
void always_on(struct LED_STATE *state);
void always_off(struct LED_STATE *state);

void app_main(void) {
    init_led();
    init_interrupts();
    xSemaphore = xSemaphoreCreateBinary();
    struct LED_STATE state = {
            LED_PIN_RED,
            false,
            esp_timer_get_time()
    };
    uint64_t total_elapsed_us = 0;
    int cycle_count = 0;
    while (1) {
        uint32_t loop_start = esp_timer_get_time();
        if (xSemaphoreTake(xSemaphore, 0) == pdTRUE) {
            ESP_LOGI(TAG, "Pattern changed -> %d", pattern);
            total_elapsed_us = 0;
            cycle_count = 0;
        }
        switch (pattern) {
            case BLINK:
                blink(&state);
                break;
            case CONSTANT_ON:
                always_on(&state);
                break;
            case CONSTANT_OFF:
                always_off(&state);
                break;
        }
        uint32_t loop_end = esp_timer_get_time();
        total_elapsed_us += (loop_end - loop_start);
        cycle_count++;

        if (cycle_count >= 1000) {
            float avg_us = (float)total_elapsed_us / 1000.0f;
            ESP_LOGI(TAG, "Avg loop time over 1000 cycles: %.2f us (%.4f ms) [pattern=%d]", avg_us, avg_us / 1000.0f, pattern);
            total_elapsed_us = 0;
            cycle_count = 0;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS); // still needed not to trigger watchdog
    }
}

void init_led() {
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = led_pin_mask;
    gpio_config(&led_conf);

    gpio_config_t button_conf = {};
    button_conf.intr_type = GPIO_INTR_NEGEDGE;
    button_conf.mode = GPIO_MODE_INPUT;
    button_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    button_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    button_conf.pin_bit_mask = (1ULL << PIN_BUTTON);
    gpio_config(&button_conf);
}

void init_interrupts() {
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON, button_isr_handler, NULL);
}

void blink(struct LED_STATE *state) {
    uint32_t current = esp_timer_get_time();
    if (current - state->last_change_microseconds > BLINK_DELAY_uS) {
        state->state = !state->state;
        state->last_change_microseconds = current;
        gpio_set_level(state->pin, state->state);
    }
}

void always_on(struct LED_STATE *state) {
    state->state = true;
    gpio_set_level(state->pin, state->state);
}

void always_off(struct LED_STATE *state) {
    state->state = false;
    gpio_set_level(state->pin, state->state);
}
