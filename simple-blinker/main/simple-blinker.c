#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "blinker";

#define LED_PIN_RED    GPIO_NUM_7
#define LED_PIN_BLUE     GPIO_NUM_8
#define PIN_BUTTON GPIO_NUM_0
#define BLINK_DELAY_MS  500

void police_blink_pattern() {
    gpio_set_level(LED_PIN_RED, 1);
    gpio_set_level(LED_PIN_BLUE, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    gpio_set_level(LED_PIN_RED, 0);
    gpio_set_level(LED_PIN_BLUE, 1);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
}

void flashing_pattern() {
    gpio_set_level(LED_PIN_RED, 1);
    gpio_set_level(LED_PIN_BLUE, 1);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    gpio_set_level(LED_PIN_RED, 0);
    gpio_set_level(LED_PIN_BLUE, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
}

void app_main(void) {
    gpio_reset_pin(LED_PIN_RED);
    gpio_reset_pin(LED_PIN_BLUE);
    gpio_reset_pin(PIN_BUTTON);
    gpio_set_direction(LED_PIN_RED, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_BLUE, GPIO_MODE_OUTPUT);

    gpio_set_direction(PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON, GPIO_PULLUP_ONLY);

    bool button_pressed = false;
    int patternIndex = 0;
    while (1) {
        if (gpio_get_level(PIN_BUTTON) == 0) {
            if (!button_pressed) {
                ESP_LOGI(TAG, "Button pressed");
            }
            button_pressed = true;
        } else {
            if (button_pressed) {
                button_pressed = false;
                patternIndex = (patternIndex + 1) % 2;
                ESP_LOGI(TAG, "Pattern changed -> %s",
                         patternIndex == 0 ? "police" : "flashing");
            }
        }
        if (patternIndex == 0) {
           police_blink_pattern();
        } else {
            flashing_pattern();
        }
    }
}


