#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char DRAM_ATTR TAG[] = "external-button";
#define EXTERNAL_BUTTON GPIO_NUM_16

static int click_count = 0;

#define DEBOUNCE_US 50000  // 50ms

static void IRAM_ATTR button_isr_handler(void *arg) {
    static int64_t last_trigger = 0;
    int64_t now = esp_timer_get_time();

    if ((now - last_trigger) < DEBOUNCE_US) return;

    last_trigger = now;
    click_count++;
    ESP_DRAM_LOGI(TAG, "Click count: %d", click_count);
}

void app_main(void) {
    gpio_reset_pin(EXTERNAL_BUTTON);
    gpio_set_direction(EXTERNAL_BUTTON, GPIO_MODE_INPUT);

    /* initialization of the GPIO ISR dispatch. MANDATORY before handler_add!!!! */
    gpio_install_isr_service(0);

    /* Will fail in install_isr_service not called before */
    gpio_set_intr_type(EXTERNAL_BUTTON, GPIO_INTR_NEGEDGE);

    gpio_isr_handler_add(EXTERNAL_BUTTON, button_isr_handler, NULL);
}
