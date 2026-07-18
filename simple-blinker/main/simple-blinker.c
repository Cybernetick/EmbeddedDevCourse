#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "blinker";
static SemaphoreHandle_t xSemaphore = NULL;

#define LED_PIN_RED_1    GPIO_NUM_6
#define LED_PIN_RED_2    GPIO_NUM_7
#define LED_PIN_BLUE_1     GPIO_NUM_8
#define LED_PIN_BLUE_2     GPIO_NUM_3
#define PIN_BUTTON GPIO_NUM_0
#define BLINK_DELAY_MS  500
#define SNAKEY_DELAY_MS  50

static void IRAM_ATTR button_isr_handler(void *arg) {
    xSemaphoreGiveFromISR(xSemaphore, NULL);
}

void police_blink_pattern() {
    gpio_set_level(LED_PIN_RED_1, 1);
    gpio_set_level(LED_PIN_RED_2, 1);
    gpio_set_level(LED_PIN_BLUE_1, 0);
    gpio_set_level(LED_PIN_BLUE_2, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    gpio_set_level(LED_PIN_RED_1, 0);
    gpio_set_level(LED_PIN_RED_2, 0);
    gpio_set_level(LED_PIN_BLUE_1, 1);
    gpio_set_level(LED_PIN_BLUE_2, 1);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
}

void flashing_pattern() {
    gpio_set_level(LED_PIN_RED_1, 1);
    gpio_set_level(LED_PIN_RED_2, 1);
    gpio_set_level(LED_PIN_BLUE_1, 1);
    gpio_set_level(LED_PIN_BLUE_2, 1);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
    gpio_set_level(LED_PIN_RED_1, 0);
    gpio_set_level(LED_PIN_RED_2, 0);
    gpio_set_level(LED_PIN_BLUE_1, 0);
    gpio_set_level(LED_PIN_BLUE_2, 0);
    vTaskDelay(pdMS_TO_TICKS(BLINK_DELAY_MS));
}

void snakey_pattern() {
    static const gpio_num_t led_pins[] = {LED_PIN_RED_1, LED_PIN_RED_2, LED_PIN_BLUE_1, LED_PIN_BLUE_2};
    static const int led_count = 4;

    for (int i = 0; i < led_count; i++) {
        gpio_set_level(led_pins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(SNAKEY_DELAY_MS));
        gpio_set_level(led_pins[i], 0);
    }
    for (int i = led_count - 2; i > 0; i--) {
        gpio_set_level(led_pins[i], 1);
        vTaskDelay(pdMS_TO_TICKS(SNAKEY_DELAY_MS));
        gpio_set_level(led_pins[i], 0);
    }
}

void app_main(void) {
    gpio_reset_pin(LED_PIN_RED_1);
    gpio_reset_pin(LED_PIN_BLUE_1);
    gpio_reset_pin(LED_PIN_RED_2);
    gpio_reset_pin(LED_PIN_BLUE_2);
    gpio_reset_pin(PIN_BUTTON);
    gpio_set_direction(LED_PIN_RED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_BLUE_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_RED_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_BLUE_2, GPIO_MODE_OUTPUT);


    gpio_set_direction(PIN_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(PIN_BUTTON, GPIO_PULLUP_ONLY);

    xSemaphore = xSemaphoreCreateBinary();
    gpio_set_intr_type(PIN_BUTTON, GPIO_INTR_NEGEDGE);
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON, button_isr_handler, NULL);

    int patternIndex = 0;
    while (1) {
        if (xSemaphoreTake(xSemaphore, 0) == pdTRUE) {
            patternIndex = (patternIndex + 1) % 3;
            ESP_LOGI(TAG, "Pattern changed -> %d", patternIndex);
        }
        if (patternIndex == 0) {
            police_blink_pattern();
        } else if (patternIndex == 1) {
            flashing_pattern();
        } else {
            snakey_pattern();
        }
    }
}


