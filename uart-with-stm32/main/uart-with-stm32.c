#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_timer.h"

#define UART_TX_PIN GPIO_NUM_16
#define UART_RX_PIN GPIO_NUM_17
#define EXTERNAL_BUTTON_PIN GPIO_NUM_15
#define LED_PIN GPIO_NUM_12
#define DEBOUNCE_MS 200

const char *COMMAND_ON = "ON\n";
const char *COMMAND_OFF = "OFF\n";

const int UART_BUFFER_SIZE = 256;
static QueueHandle_t uart_queue;
static bool last_sent_pin_state = false;
static volatile bool button_pressed = false;

void init_gpio(void);
void init_interrupts(void);
void setup_uart(void);

_Noreturn void rx_task(void *pvParameters);
_Noreturn void tx_task(void *pvParameters);

void IRAM_ATTR gpio_isr_handler(void *arg) {
    static int64_t last_press_time = 0;
    int64_t now = esp_timer_get_time();
    if (now - last_press_time > DEBOUNCE_MS * 1000) {
        button_pressed = true;
        last_press_time = now;
    }
}

void app_main(void)
{
    init_gpio();
    init_interrupts();
    setup_uart();
    xTaskCreate(rx_task, "rx_task", 2048, NULL, 10, NULL);
    xTaskCreate(tx_task, "tx_task", 2048, NULL, 10, NULL);
}

_Noreturn void tx_task(void *pvParameters) {
    while (1) {
        if(button_pressed) {
            button_pressed = false;
            if (last_sent_pin_state) {
                uart_write_bytes(UART_NUM_1, COMMAND_OFF, strlen(COMMAND_OFF));
                printf("[TX] Sent: %s\n", COMMAND_OFF);
            } else {
                uart_write_bytes(UART_NUM_1, COMMAND_ON, strlen(COMMAND_ON));
                printf("[TX] Sent: %s\n", COMMAND_ON);
            }
            last_sent_pin_state = !last_sent_pin_state;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

_Noreturn void rx_task(void *pvParameters) {
    char buffer[30];
    while (1) {
        int length_available = 0;
        ESP_ERROR_CHECK(uart_get_buffered_data_len(UART_NUM_1, (size_t*) &length_available));
        if (length_available >= 30){
            printf("[RX] Buffer overflow (%d bytes), flushing\n", length_available);
            uart_flush(UART_NUM_1);
        } else if (length_available > 0 ) {
            int bytes_read = uart_read_bytes(UART_NUM_1, buffer, length_available, 1000 / portTICK_PERIOD_MS);
            buffer[bytes_read] = '\0';
            if (bytes_read > 0 && buffer[bytes_read - 1] == '\n') buffer[--bytes_read] = '\0';
            if (bytes_read > 0 && buffer[bytes_read - 1] == '\r') buffer[--bytes_read] = '\0';
            printf("[RX] Received %d bytes: '%s' | hex:", bytes_read, buffer);
            for (int i = 0; i < bytes_read; i++) printf(" %02X", (unsigned char)buffer[i]);
            printf("\n");
            if (strcmp(buffer, "ON") == 0) {
                gpio_set_level(LED_PIN, 1);
                printf("[RX] LED ON\n");
            } else if (strcmp(buffer, "OFF") == 0) {
                gpio_set_level(LED_PIN, 0);
                printf("[RX] LED OFF\n");
            } else {
                printf("[RX] Unknown command\n");
            }
            memset(buffer, 0, sizeof(buffer));
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void setup_uart(void) {
    uart_port_t uart_port = UART_NUM_1;
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(uart_port, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_port, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, UART_BUFFER_SIZE, UART_BUFFER_SIZE, 5, &uart_queue, 0));
}

void init_gpio() {
    gpio_config_t led_conf = {
            .pin_bit_mask = (1ULL << LED_PIN),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&led_conf);

    gpio_config_t button_conf = {
            .pin_bit_mask = (1ULL << EXTERNAL_BUTTON_PIN),
            .mode = GPIO_MODE_INPUT,
            .pull_up_en = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_NEGEDGE
    };
    gpio_config(&button_conf);
}

void init_interrupts() {
    gpio_install_isr_service(0);
    gpio_isr_handler_add(EXTERNAL_BUTTON_PIN, gpio_isr_handler, NULL);
}