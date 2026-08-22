#include <esp_timer.h>
#include "driver/gpio.h"

#define ON_US       (5ULL  * 1000000)
#define OFF_US      (20ULL * 1000000)
static const char *TAG = "timbers";
constexpr int control_pin = (1ULL << GPIO_NUM_4);

enum ControlSignalState {
    STATE_ON,
    STATE_OFF,
};

static enum ControlSignalState control_signal_state = STATE_OFF;
static esp_timer_handle_t control_relay_timer;

void init_timer(void);

void app_main(void)
{
    gpio_config_t led_conf = {};
    led_conf.intr_type = GPIO_INTR_DISABLE;
    led_conf.mode = GPIO_MODE_OUTPUT;
    led_conf.pin_bit_mask = control_pin;
    gpio_config(&led_conf);
    init_timer();
}

static void control_timer_cb(void *arg) {
    if (control_signal_state == STATE_ON) {
        control_signal_state = STATE_OFF;
        gpio_set_level(GPIO_NUM_4, 0);
        esp_timer_start_once(control_relay_timer, OFF_US);
    } else {
        control_signal_state = STATE_ON;
        gpio_set_level(GPIO_NUM_4, 1);
        esp_timer_start_once(control_relay_timer, ON_US);
    }
}

void init_timer() {
    esp_timer_create_args_t timer_args = {
            .callback = control_timer_cb,
            .name     = "control_relay_timer"
    };
    esp_timer_create(&timer_args, &control_relay_timer);
    esp_timer_start_once(control_relay_timer, OFF_US);
}
