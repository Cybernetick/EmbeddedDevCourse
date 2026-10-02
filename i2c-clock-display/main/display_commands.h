//
// Created by Ivan Khulup on 01.10.2026.
//

#ifndef I2C_CLOCK_DISPLAY_DISPLAY_COMMANDS_H
#define I2C_CLOCK_DISPLAY_DISPLAY_COMMANDS_H
#include <esp_err.h>
#include <driver/i2c_master.h>

#define I2C_DISPLAY_ADDR 0x3C //or maybe 0x3D, need verification
#define DISPLAY_NUM_CHARS 8
struct command_t {
    uint8_t command;
    uint8_t argc;
    uint8_t args[2];
};

void oled_send_cmd(i2c_master_dev_handle_t device_handle, struct command_t command);
void oled_init(i2c_master_dev_handle_t device_handle);
void oled_clear(i2c_master_dev_handle_t device_handle);
void oled_draw_time(i2c_master_dev_handle_t device_handle, uint8_t hours, uint8_t minutes, uint8_t seconds);
void oled_draw_date(i2c_master_dev_handle_t device_handle, uint8_t dom, uint16_t year);
#endif //I2C_CLOCK_DISPLAY_DISPLAY_COMMANDS_H
