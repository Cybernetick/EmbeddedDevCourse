//
// Created by Ivan Khulup on 02.10.2026.
//

#ifndef I2C_CLOCK_DISPLAY_CLOCK_COMMANDS_H
#define I2C_CLOCK_DISPLAY_CLOCK_COMMANDS_H

#include <driver/i2c_master.h>

#define DS_CLOCK_ADDRESS 0x68
#define DS_SDRAM_FLAG_ADDRESS 0x0A
#define DS_TIME_SET_FLAG_VALUE 0xAA

struct ds1307_time_t {
    uint8_t sec;
    uint8_t min;
    uint8_t hour;
    uint8_t dow;
    uint8_t day;
    uint8_t month;
    uint16_t year;
};

static inline uint8_t bcd2dec(uint8_t v) { return (v >> 4) * 10 + (v & 0x0F); }
static inline uint8_t dec2bcd(uint8_t v) { return ((v / 10) << 4) | (v % 10); }

void ds1307_set_time(i2c_master_dev_handle_t device_handle, struct ds1307_time_t time);
esp_err_t ds1307_read_time(i2c_master_dev_handle_t device_handle, struct ds1307_time_t* result);
esp_err_t ds1307_is_time_set(i2c_master_dev_handle_t device_handle);
#endif //I2C_CLOCK_DISPLAY_CLOCK_COMMANDS_H
