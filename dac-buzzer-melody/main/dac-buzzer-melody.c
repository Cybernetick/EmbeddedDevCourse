#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <soc/gpio_num.h>
#include <driver/ledc.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <stdint-gcc.h>
#include "notes.h"

const gpio_num_t buzzer_pin = 8;

struct Note {
    uint32_t frequency;
    int duration;
};

struct Note melody[] = {
        {REST,     2},
        {NOTE_D4,  4},
        {NOTE_G4,  -4},
        {NOTE_AS4, 8},
        {NOTE_A4,  4},
        {NOTE_G4,  2},
        {NOTE_D5,  4},
        {NOTE_C5,  -2},
        {NOTE_A4,  -2},
        {NOTE_G4,  -4},
        {NOTE_AS4, 8},
        {NOTE_A4,  4},
        {NOTE_F4,  2},
        {NOTE_GS4, 4},
        {NOTE_D4,  -1},
        {NOTE_D4,  4},

        {NOTE_G4,  -4},
        {NOTE_AS4, 8},
        {NOTE_A4,  4},
        {NOTE_G4,  2},
        {NOTE_D5,  4},
        {NOTE_F5,  2},
        {NOTE_E5,  4},
        {NOTE_DS5, 2},
        {NOTE_B4,  4},
        {NOTE_DS5, -4},
        {NOTE_D5,  8},
        {NOTE_CS5, 4},
        {NOTE_CS4, 2},
        {NOTE_B4,  4},
        {NOTE_G4,  -1},
        {NOTE_AS4, 4},
        {NOTE_D5,  2},
        {NOTE_AS4, 4},
        {NOTE_D5,  2},
        {NOTE_AS4, 4},
        {NOTE_DS5, 2},
        {NOTE_D5,  4},
        {NOTE_CS5, 2},
        {NOTE_A4,  4},
        {NOTE_AS4, -4},
        {NOTE_D5,  8},
        {NOTE_CS5, 4},
        {NOTE_CS4, 2},
        {NOTE_D4,  4},
        {NOTE_D5,  -1},
        {REST,     4},
        {NOTE_AS4, 4},
        {NOTE_D5,  2},
        {NOTE_AS4, 4},
        {NOTE_D5,  2},
        {NOTE_AS4, 4},
        {NOTE_F5,  2},
        {NOTE_E5,  4},
        {NOTE_DS5, 2},
        {NOTE_B4,  4},
        {NOTE_DS5, -4},
        {NOTE_D5,  8},
        {NOTE_CS5, 4},
        {NOTE_CS4, 2},
        {NOTE_AS4, 4},
        {NOTE_G4,  -1}
};

constexpr int tempo = 144;
constexpr int whole_note_duration = (60000 * 4) / tempo;
constexpr size_t melody_length = sizeof(melody) / sizeof(melody[0]);
static int current_note = 0;

void play_note(uint32_t frequency, uint32_t duration);

_Noreturn void playback_task(void *pvParameters);

void playback_no_delay(void);

void app_main(void) {
    ledc_timer_config_t ledc_timer = {
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_num = LEDC_TIMER_0,
            .duty_resolution = LEDC_TIMER_10_BIT,
            .freq_hz = 1000,
            .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
            .channel = LEDC_CHANNEL_0,
            .duty = 0,
            .gpio_num = buzzer_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .timer_sel = LEDC_TIMER_0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

//    xTaskCreate(playback_task, "playback_task", 4096, NULL, 5, NULL);
    playback_no_delay();
}

void playback_no_delay(void) {
    uint64_t current_note_start_ts = esp_timer_get_time() / 1000;
    float currently_playing_note_duration = 0;
    while (1) {
        float note_duration = 0;
        if (melody[current_note].duration > 0) {
            note_duration = (float) whole_note_duration / (float) melody[current_note].duration;
        } else if (melody[current_note].duration < 0) {
            note_duration = (float) whole_note_duration / (float) abs(melody[current_note].duration);
            note_duration *= 1.5;
        }
        uint64_t elapsed_time = esp_timer_get_time() / 1000 - current_note_start_ts;
        if (elapsed_time >= currently_playing_note_duration) {
            currently_playing_note_duration = note_duration;
            current_note_start_ts = esp_timer_get_time() / 1000;
            if (melody[current_note].frequency == REST) {
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            } else {
                ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, melody[current_note].frequency));
                ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512));
                ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
            }
            current_note = (current_note + 1) % melody_length;
        } else if (elapsed_time >= currently_playing_note_duration * 0.9) {
            ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
            ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

_Noreturn void playback_task(void *pvParameters) {
    while (1) {
        for (int i = 0; i < melody_length; ++i) {
            float note_duration = 0;
            if (melody[i].duration > 0) {
                note_duration = (float) whole_note_duration / (float) melody[i].duration;
            } else if (melody[i].duration < 0) {
                note_duration = (float) whole_note_duration / (float) abs(melody[i].duration);
                note_duration *= 1.5;
            }
            play_note(melody[i].frequency, (uint32_t) note_duration);
        }
        ESP_LOGI("playback_task", "playback finished, reseting");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void play_note(uint32_t frequency, uint32_t duration) {
    if (frequency == REST) {
        vTaskDelay(pdMS_TO_TICKS(duration));
        return;
    }
    ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, LEDC_TIMER_0, frequency));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 512));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
    vTaskDelay(pdMS_TO_TICKS(duration));
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0));
}