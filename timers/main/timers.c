#include <stdio.h>
#include <esp_timer.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "traffic-lights";

constexpr gpio_num_t LED_RED    = GPIO_NUM_4;
constexpr gpio_num_t LED_YELLOW = GPIO_NUM_7;
constexpr gpio_num_t LED_GREEN  = GPIO_NUM_8;
constexpr gpio_num_t PIN_BUTTON = GPIO_NUM_11;

constexpr int led_pin_mask = (1ULL << LED_RED) | (1ULL << LED_YELLOW) | (1ULL << LED_GREEN);

enum TrafficLightState {
    STATE_RED,
    STATE_RED_YELLOW,
    STATE_GREEN,
    STATE_GREEN_FLASHING,
    STATE_YELLOW,
    STATE_FLASHING_YELLOW,
};
constexpr int STATE_COUNT = 5;

// duration each state is held, in microseconds
constexpr uint64_t STATE_DURATION_US[] = {
    [STATE_RED]            = 4000000,
    [STATE_RED_YELLOW]     = 1000000,
    [STATE_GREEN]          = 4000000,
    [STATE_GREEN_FLASHING] = 3000000, // 3 flashes × 1000ms
    [STATE_YELLOW]         = 1000000,
};

volatile enum TrafficLightState traffic_state = STATE_RED;

static esp_timer_handle_t traffic_timer;
static TaskHandle_t traffic_task_handle;

void init_pins(void);
void init_button(void);
void init_timer(void);
_Noreturn void traffic_light_task(void *pvParameters);

static void IRAM_ATTR button_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    traffic_state = (traffic_state != STATE_FLASHING_YELLOW) ? STATE_FLASHING_YELLOW : STATE_RED;
    xTaskNotifyFromISR(traffic_task_handle, 0, eNoAction, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

static void traffic_timer_cb(void *arg) {
    if (traffic_state == STATE_FLASHING_YELLOW) return;
    traffic_state = (enum TrafficLightState)((traffic_state + 1) % STATE_COUNT);
    esp_timer_start_once(traffic_timer, STATE_DURATION_US[traffic_state]);
}

void app_main(void) {
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_BUTTON, button_isr_handler, NULL);

    init_pins();
    init_timer();

    xTaskCreate(traffic_light_task, "traffic_light", 2048, NULL, 5, &traffic_task_handle);
}

void init_timer() {
    esp_timer_create_args_t timer_args = {
        .callback = traffic_timer_cb,
        .name     = "traffic_timer"
    };
    esp_timer_create(&timer_args, &traffic_timer);
    esp_timer_start_once(traffic_timer, STATE_DURATION_US[traffic_state]);
}

_Noreturn void traffic_light_task(void *pvParameters) {
    enum TrafficLightState last_state = -1;
    while (1) {
        enum TrafficLightState current = traffic_state;

        if (current != last_state) {
            last_state = current;
            ESP_LOGI(TAG, "State -> %d", current);

            switch (current) {
                case STATE_RED:
                    gpio_set_level(LED_RED,    1);
                    gpio_set_level(LED_YELLOW, 0);
                    gpio_set_level(LED_GREEN,  0);
                    break;
                case STATE_RED_YELLOW:
                    gpio_set_level(LED_RED,    1);
                    gpio_set_level(LED_YELLOW, 1);
                    gpio_set_level(LED_GREEN,  0);
                    break;
                case STATE_GREEN:
                    gpio_set_level(LED_RED,    0);
                    gpio_set_level(LED_YELLOW, 0);
                    gpio_set_level(LED_GREEN,  1);
                    break;
                case STATE_GREEN_FLASHING:
                    gpio_set_level(LED_RED,    0);
                    gpio_set_level(LED_YELLOW, 0);
                    for (int i = 0; i < 3 && traffic_state == STATE_GREEN_FLASHING; i++) {
                        gpio_set_level(LED_GREEN, 1);
                        vTaskDelay(pdMS_TO_TICKS(500));
                        gpio_set_level(LED_GREEN, 0);
                        vTaskDelay(pdMS_TO_TICKS(500));
                    }
                    break;
                case STATE_YELLOW:
                    gpio_set_level(LED_RED,    0);
                    gpio_set_level(LED_YELLOW, 1);
                    gpio_set_level(LED_GREEN,  0);
                    break;
                case STATE_FLASHING_YELLOW:
                    gpio_set_level(LED_RED,   0);
                    gpio_set_level(LED_GREEN, 0);
                    while (traffic_state == STATE_FLASHING_YELLOW) {
                        gpio_set_level(LED_YELLOW, 1);
                        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
                        gpio_set_level(LED_YELLOW, 0);
                        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(500));
                    }
                    esp_timer_start_once(traffic_timer, STATE_DURATION_US[traffic_state]);
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void init_pins(void) {
    gpio_config_t led_conf = {};
    led_conf.intr_type    = GPIO_INTR_DISABLE;
    led_conf.mode         = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = led_pin_mask;
    gpio_config(&led_conf);

    gpio_config_t btn_conf = {};
    btn_conf.intr_type    = GPIO_INTR_NEGEDGE;
    btn_conf.mode         = GPIO_MODE_INPUT;
    btn_conf.pull_up_en   = GPIO_PULLUP_DISABLE;
    btn_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    btn_conf.pin_bit_mask = (1ULL << PIN_BUTTON);
    gpio_config(&btn_conf);
}
