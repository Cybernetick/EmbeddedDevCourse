#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "external-button";
static QueueHandle_t xButtonQueue;
#define LED_PIN_RED_1    GPIO_NUM_6

#define LED_PIN_RED_2    GPIO_NUM_7
#define LED_PIN_BLUE_1     GPIO_NUM_8
#define LED_PIN_BLUE_2     GPIO_NUM_3
#define BOOT_BUTTON GPIO_NUM_0
#define EXTERNAL_BUTTON GPIO_NUM_16
#define BLINK_DELAY_MS  500
#define SNAKEY_DELAY_MS  50
#define SNAKE_PATTERN 0
#define POLICE_PATTERN 1

static volatile int current_pattern = SNAKE_PATTERN;

static void IRAM_ATTR button_isr_handler(void *arg) {
    gpio_num_t pin = (gpio_num_t)(uint32_t)arg;
    xQueueSendFromISR(xButtonQueue, &pin, NULL);
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

void blinker_task(void *pvParameters) {
    while (1) {
        if (current_pattern == POLICE_PATTERN) {
            ESP_LOGI(TAG, "police blinking");
            police_blink_pattern();
        } else {
            ESP_LOGI(TAG, "snake pattern");
            snakey_pattern();
        }
    }
}

void button_task(void *pvParameters) {
    gpio_num_t pin;
    while (1) {
        if (xQueueReceive(xButtonQueue, &pin, portMAX_DELAY)) {
            ESP_LOGI(TAG, "Button %d pressed", pin);
            if (pin == BOOT_BUTTON) current_pattern = POLICE_PATTERN;
            if (pin == EXTERNAL_BUTTON) current_pattern = SNAKE_PATTERN;
        }
    }

}
void app_main(void) {
    xButtonQueue = xQueueCreate(10, sizeof(gpio_num_t));

    gpio_reset_pin(LED_PIN_RED_1);
    gpio_reset_pin(LED_PIN_BLUE_1);
    gpio_reset_pin(LED_PIN_RED_2);
    gpio_reset_pin(LED_PIN_BLUE_2);

    gpio_set_direction(LED_PIN_RED_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_BLUE_1, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_RED_2, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_PIN_BLUE_2, GPIO_MODE_OUTPUT);

    gpio_reset_pin(EXTERNAL_BUTTON);
    gpio_set_direction(EXTERNAL_BUTTON, GPIO_MODE_INPUT);

    gpio_reset_pin(BOOT_BUTTON);
    gpio_set_direction(BOOT_BUTTON, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BOOT_BUTTON, GPIO_PULLUP_ONLY);

    /* initialization of the GPIO ISR dispatch. MANDATORY before handler_add!!!! */
    gpio_install_isr_service(0);

    /* Will fail in install_isr_service not called before */
    gpio_set_intr_type(BOOT_BUTTON,     GPIO_INTR_NEGEDGE);
    gpio_set_intr_type(EXTERNAL_BUTTON, GPIO_INTR_NEGEDGE);

    gpio_isr_handler_add(BOOT_BUTTON,     button_isr_handler, (void *)BOOT_BUTTON);
    gpio_isr_handler_add(EXTERNAL_BUTTON, button_isr_handler, (void *)EXTERNAL_BUTTON);

    xTaskCreate(&button_task, "button_task", 2048, NULL, 10, NULL);
    xTaskCreate(&blinker_task, "blinker_task", 2048, NULL, 10, NULL);
}
