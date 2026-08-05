#include <stdio.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

constexpr gpio_num_t LED_PIN_RED = GPIO_NUM_6;
constexpr gpio_num_t LED_PIN_GREEN = GPIO_NUM_7;
constexpr gpio_num_t LED_PIN_BLUE = GPIO_NUM_8;

struct LED_STATE {
    gpio_num_t pin;
    bool state;
    uint32_t last_change_microseconds;
    uint32_t blink_interval_microseconds;
};

constexpr int led_pin_mask = (1ULL << LED_PIN_RED | 1ULL << LED_PIN_GREEN | 1ULL << LED_PIN_BLUE);

void init_leds(void)
{
    gpio_config_t led_config = {
        .pin_bit_mask = led_pin_mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&led_config);
}
void blink_led(struct LED_STATE *led)
{
    uint32_t now_microseconds = esp_timer_get_time();

    if (led->last_change_microseconds + led->blink_interval_microseconds > now_microseconds) {
        return;
    }
    led->state = !led->state;
    gpio_set_level(led->pin, led->state);
    led->last_change_microseconds = now_microseconds;
}

void app_main(void)
{
    init_leds();
    struct LED_STATE leds[3] = {
        {LED_PIN_RED, false, 0, 1000000},
        {LED_PIN_GREEN, false, 0, 500000},
        {LED_PIN_BLUE, false, 0, 200000},
    };
    while (1) {
        for (int i = 0; i < 3; i++) {
            blink_led(&leds[i]);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
