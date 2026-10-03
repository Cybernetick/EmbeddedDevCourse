#include <stdio.h>
#include <esp_log.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "display_commands.h"
#include "clock_commands.h"

#define I2C_CLOCK_PIN GPIO_NUM_9
#define I2C_DATA_PIN GPIO_NUM_8
#define I2C_PORT I2C_NUM_0

static i2c_master_bus_handle_t i2c_bus = NULL;
static i2c_master_dev_handle_t display_handle = NULL;
static i2c_master_dev_handle_t clock_handle = NULL;
void init_i2c_bus(void);
void init_i2c_display(void);
void init_i2c_clock(void);
void oled_control_task(void *pvParameters);
void ds1307_clock_reading_task(void *pvParameters);

volatile struct ds1307_time_t time;

void app_main(void) {
    init_i2c_bus();
    init_i2c_display();
    init_i2c_clock();
    esp_err_t probe_result = i2c_master_probe(i2c_bus, I2C_DISPLAY_ADDR, 1000);
    if (probe_result != ESP_OK) {
        printf("I2C display not found at address 0x%02x\n", I2C_DISPLAY_ADDR);
        return;
    }

    probe_result = i2c_master_probe(i2c_bus, DS_CLOCK_ADDRESS, 1000);
    if (probe_result != ESP_OK) {
        printf("I2C clock not found at address 0x%02x\n", DS_CLOCK_ADDRESS);
        return;
    }
    esp_err_t is_clock_set = ds1307_is_time_set(clock_handle);
    if (is_clock_set != ESP_OK) {
        ds1307_set_time(clock_handle, (struct ds1307_time_t){ 0, 30, 22, 5, 2, 10, 2026 });
    }
    xTaskCreate(oled_control_task, "oled_control_task", 4096, NULL, 10, NULL);
    xTaskCreate(ds1307_clock_reading_task, "ds1307_clock_reading_task", 4096, NULL, 10, NULL);
}

void oled_control_task(void *pvParameters) {
    ESP_LOGI("i2c_clock_display", "Starting OLED control task");
    oled_init(display_handle);
    vTaskDelay(pdMS_TO_TICKS(1000));
    oled_clear(display_handle);
    while (1) {
        oled_draw_time(display_handle, time.hour, time.min, time.sec);
        oled_draw_date(display_handle, time.day, time.month, time.year);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void ds1307_clock_reading_task(void *pvParameters) {
    ESP_LOGI("i2c_clock_display", "Starting DS1307 clock reading task");
    struct ds1307_time_t time_local;
    while (1) {
        ESP_ERROR_CHECK(ds1307_read_time(clock_handle, &time_local));
        time = time_local;
        ESP_LOGI("i2c_clock_display", "Current time: %d:%d:%d %d/%d/%d", time.hour, time.min, time.sec, time.day, time.month, time.year);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void init_i2c_bus(void) {
    i2c_master_bus_config_t master_bus_config = {
            .clk_source = I2C_CLK_SRC_DEFAULT,
            .i2c_port = I2C_PORT,
            .sda_io_num = I2C_DATA_PIN,
            .scl_io_num = I2C_CLOCK_PIN,
            .flags = {
                    .enable_internal_pullup = 1
            }
    };
    ESP_ERROR_CHECK(i2c_new_master_bus( &master_bus_config, &i2c_bus));
}

void init_i2c_display(void) {
    i2c_device_config_t device_config = {
            .device_address = I2C_DISPLAY_ADDR,
            .scl_speed_hz = 100000
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &device_config, &display_handle));
}

void init_i2c_clock(void) {
    i2c_device_config_t device_config = {
            .device_address = DS_CLOCK_ADDRESS,
            .scl_speed_hz = 100000
    };
    ESP_ERROR_CHECK(i2c_master_bus_add_device(i2c_bus, &device_config, &clock_handle));
}
