#pragma once
#include <Adafruit_NeoPixel.h>

enum StripMode {
    STRIP_OFF, STRIP_IDLE, STRIP_WORKING, STRIP_APPROVAL, STRIP_OFFLINE,
    STRIP_LIGHT_BRIGHTNESS, STRIP_LIGHT_CCT, STRIP_SCENES, STRIP_LOW_BATTERY
};
void strip_init(void);
void strip_set_state(StripMode mode, uint8_t brightness, uint16_t temperature,
                     uint32_t one_shot_revision = 0);
void strip_set_budget(uint8_t max_scale);
void strip_start(unsigned long currentMillis);
