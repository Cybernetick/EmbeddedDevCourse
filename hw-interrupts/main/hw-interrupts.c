#include <stdio.h>
#include <esp_timer.h>
#include <stdint-gcc.h>
#include <inttypes.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define DEBOUNCE_US 50000LL
#define BUTTON_PIN GPIO_NUM_4

static const char *TAG = "pwn_blinker";
constexpr int external_button_pin_maks = (1ULL << BUTTON_PIN);

static volatile uint8_t button_press_count = 0;
static volatile int64_t last_isr_time_us = 0;
static volatile bool interrupt_fired = false;

static bool button_pressed = false;

static QueueHandle_t xButtonQueue;  // carries int64_t ISR timestamps

void init_button(void);
void init_interrupts(void);
_Noreturn void input_reader_task(void *pvParameters);
_Noreturn void input_reader_task_sw_debounce(void *pvParameters);
_Noreturn void input_reader_task_state_machine(void *pvParameters);
_Noreturn void input_reader_task_no_interrupt(void *pvParameters);

static void IRAM_ATTR button_isr_handler_simple(void *arg) {
    ESP_DRAM_LOGI("ISR", "interrupt fired!");
    button_press_count++;
}

static void IRAM_ATTR button_isr_handler_with_software_debounce(void *arg) {
    ESP_DRAM_LOGI("ISR", "interrupt fired!");
    int64_t now = esp_timer_get_time();
    xQueueSendFromISR(xButtonQueue, &now, NULL);
}

static void IRAM_ATTR button_isr_handler_state_machine(void *arg) {
    ESP_DRAM_LOGI("ISR", "interrupt fired!");
    interrupt_fired = true;
}

void app_main(void) {
    xButtonQueue = xQueueCreate(10, sizeof(int64_t));
    init_button();
    init_interrupts();
    xTaskCreate(input_reader_task_no_interrupt, "input_reader", 2048, NULL, 5, NULL);
}

_Noreturn void input_reader_task(void *pvParameters) {
    static uint8_t last_pressed = 0;
    while (1) {
        if (last_pressed != button_press_count) {
            ESP_LOGI(TAG, "Button press count: %d", button_press_count);
            last_pressed = button_press_count;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

_Noreturn void input_reader_task_sw_debounce(void *pvParameters) {
    int64_t isr_time_us;
    while (1) {
        if (xQueueReceive(xButtonQueue, &isr_time_us, portMAX_DELAY) == pdTRUE) {
            if (isr_time_us - last_isr_time_us > DEBOUNCE_US) {
                last_isr_time_us = isr_time_us;
                button_press_count++;
                ESP_LOGI(TAG, "Button press count: %d", button_press_count);
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

_Noreturn void input_reader_task_state_machine(void *pvParameters) {
    static uint8_t last_pressed = 0;
    while (1) {
        if (interrupt_fired) {
            int level = gpio_get_level(BUTTON_PIN);
            if (level == 1) {
                interrupt_fired = false;
                button_press_count++;
                ESP_LOGI(TAG, "Button press count: %d", button_press_count);
            }
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

_Noreturn void input_reader_task_no_interrupt(void *pvParameters) {
    while (1) {
        int level = gpio_get_level(BUTTON_PIN);
        if (level == 1 && !button_pressed) {
            button_pressed = true;
            button_press_count++;
            ESP_LOGI(TAG, "Button press count: %d", button_press_count);
        } else if (level ==0) {
            button_pressed = false;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void init_button() {
    gpio_config_t button_conf = {};
    button_conf.intr_type = GPIO_INTR_NEGEDGE;
    button_conf.mode = GPIO_MODE_INPUT;
    button_conf.pin_bit_mask = external_button_pin_maks;
    gpio_config(&button_conf);
}

void init_interrupts() {
    gpio_install_isr_service(0);
    gpio_isr_handler_add(BUTTON_PIN, button_isr_handler_state_machine, (void *) ((uint32_t) BUTTON_PIN));
}
