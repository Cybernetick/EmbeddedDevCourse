#include <stdio.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define DEBOUNCE_US 50000LL

static volatile int64_t last_isr_time_us = 0;
static volatile int64_t control_changed_us = 0;

static const char *TAG = "pwn_blinker";
constexpr int control_pin = (1ULL << GPIO_NUM_4);
constexpr int RELAY_OUTPUT_PIN_MASK = (1ULL << GPIO_NUM_11);

static QueueHandle_t xButtonQueue;  // carries int64_t ISR timestamps

void init_led(void);

void init_interrupts(void);

void control_signal_task(void *pvParameters);

void input_reader_task(void *pvParameters);

static void IRAM_ATTR button_isr_handler(void *arg) {
    int64_t now = esp_timer_get_time();
    if (now - last_isr_time_us < DEBOUNCE_US) return;
    last_isr_time_us = now;

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(xButtonQueue, &now, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void app_main(void) {
    xButtonQueue = xQueueCreate(10, sizeof(int64_t));
    init_led();
    init_interrupts();
    xTaskCreate(control_signal_task, "control_signal", 2048, NULL, 5, NULL);
    xTaskCreate(input_reader_task, "input_reader", 2048, NULL, 5, NULL);
}

void input_reader_task(void *pvParameters) {
    int64_t isr_time_us;
    while (1) {
        if (xQueueReceive(xButtonQueue, &isr_time_us, portMAX_DELAY)) {
            int64_t response_us = isr_time_us - control_changed_us;
            uint32_t level = gpio_get_level(GPIO_NUM_11);
            ESP_LOGI(TAG, "pin 11 level %d | relay response: %lld us (%lld ms)", level, response_us, response_us / 1000);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void control_signal_task(void *pvParameters) {
    int level = 0;
    while (1) {
        gpio_set_level(GPIO_NUM_4, level);
        control_changed_us = esp_timer_get_time();
        ESP_LOGI(TAG, "control level %d", level);
        level = !level;
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void init_led() {
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = control_pin;
    gpio_config(&led_conf);

    gpio_config_t nc_conf = {};
    nc_conf.intr_type = GPIO_INTR_ANYEDGE;
    nc_conf.mode = GPIO_MODE_INPUT;
    nc_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    nc_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    nc_conf.pin_bit_mask = RELAY_OUTPUT_PIN_MASK;
    gpio_config(&nc_conf);
}

void init_interrupts() {
    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_NUM_11, button_isr_handler, (void *) ((uint32_t) GPIO_NUM_11));
}
