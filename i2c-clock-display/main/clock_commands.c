//
// Created by Ivan Khulup on 02.10.2026.
//
#include "clock_commands.h"


void ds1307_set_time(i2c_master_dev_handle_t device_handle, struct ds1307_time_t time) {
    uint8_t data[8];
    data[0] = 0x00;
    data[1] = dec2bcd(time.sec);
    data[2] = dec2bcd(time.min);
    data[3] = dec2bcd(time.hour);
    data[4] = dec2bcd(time.dow);
    data[5] = dec2bcd(time.day);
    data[6] = dec2bcd(time.month);
    data[7] = dec2bcd(time.year % 100);
    ESP_ERROR_CHECK(i2c_master_transmit(device_handle, data, sizeof(data), 100));

    uint8_t time_set[2];
    time_set[0] = DS_SDRAM_FLAG_ADDRESS;
    time_set[1] = DS_TIME_SET_FLAG_VALUE;
    ESP_ERROR_CHECK(i2c_master_transmit(device_handle, time_set, sizeof(time_set), 100));
}

esp_err_t ds1307_read_time(i2c_master_dev_handle_t device_handle, struct ds1307_time_t* result) {
    uint8_t reg = 0x00;
    uint8_t data[7];

    esp_err_t err = i2c_master_transmit_receive(device_handle, &reg, 1, data, sizeof(data), 1000);
    if (err != ESP_OK)
        return err;

    result->sec   = bcd2dec(data[0] & 0x7F);
    result->min   = bcd2dec(data[1] & 0x7F);
    result->hour  = bcd2dec(data[2] & 0x3F);
    result->dow   = bcd2dec(data[3] & 0x07);
    result->day   = bcd2dec(data[4] & 0x3F);
    result->month = bcd2dec(data[5] & 0x1F);
    result->year  = bcd2dec(data[6]) + 2000;

    return ESP_OK;
}

esp_err_t ds1307_is_time_set(i2c_master_dev_handle_t device_handle) {
    uint8_t data[1];
    uint8_t addr = DS_SDRAM_FLAG_ADDRESS;
    esp_err_t read_result = i2c_master_transmit_receive(device_handle, &addr, 1, data, sizeof(data), 1000);
    if (read_result != ESP_OK) return read_result;
    if (data[0] == DS_TIME_SET_FLAG_VALUE) return ESP_OK;
    return ESP_ERR_NOT_FOUND;
}