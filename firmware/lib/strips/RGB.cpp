#include "RGB.h"
#include <Arduino.h>

namespace {
constexpr uint8_t LED_PIN = 38;
constexpr uint8_t LED_COUNT = 8;
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);
portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
StripMode mode = STRIP_IDLE;
uint8_t light_level = 50;
uint16_t cct = 4000;
uint32_t event_revision = 0;
uint32_t played_event = 0;
uint32_t event_started = 0;
uint32_t previous_frame = 0;
uint8_t budget = 255;
uint8_t frame_budget = 255;
bool dark_cleared = false;

uint8_t wave(uint32_t now, uint32_t period) {
    uint32_t t = now % period;
    uint32_t half = period / 2;
    return 4 + 10 * (t < half ? t : period-t) / half;
}

void color(uint8_t index, uint8_t r, uint8_t g, uint8_t b) {
    strip.setPixelColor(index, strip.Color(r*frame_budget/255,g*frame_budget/255,b*frame_budget/255));
}
}

void strip_init(void) {
    strip.begin();
    strip.clear();
    strip.show();
}

void strip_set_state(StripMode next, uint8_t brightness, uint16_t temperature,
                     uint32_t one_shot_revision) {
    portENTER_CRITICAL(&state_mux);
    mode = next;
    light_level = brightness;
    cct = temperature;
    event_revision = one_shot_revision;
    portEXIT_CRITICAL(&state_mux);
}

void strip_set_budget(uint8_t max_scale) {
    portENTER_CRITICAL(&state_mux);
    budget = max_scale;
    portEXIT_CRITICAL(&state_mux);
}

void strip_start(unsigned long now) {
    if (now - previous_frame < 40) return; // Sole NeoPixel writer, 25 fps.
    previous_frame = now;
    StripMode current;
    uint8_t brightness;
    uint16_t temperature;
    uint32_t revision;
    portENTER_CRITICAL(&state_mux);
    current = mode;
    brightness = light_level;
    temperature = cct;
    revision = event_revision;
    frame_budget = budget;
    portEXIT_CRITICAL(&state_mux);
    if (frame_budget == 0) {
        if (!dark_cleared) { strip.clear(); strip.show(); dark_cleared = true; }
        return; // keep WS2812 black without continuous 25fps bus traffic
    }
    dark_cleared = false;
    if (revision != played_event) { played_event = revision; event_started = now; }
    strip.clear();
    uint8_t level = wave(now, 6000);
    for (uint8_t i=0; i<LED_COUNT; ++i) {
        switch (current) {
        case STRIP_LIGHT_BRIGHTNESS: {
            uint8_t amount = 5 + (brightness * 38 / 100);
            color(i, amount, amount * 3 / 4, amount / 2);
            break;
        }
        case STRIP_LIGHT_CCT: {
            uint8_t cold = (temperature - 2700) * 36 / 3800;
            color(i, 42-cold, 18+cold/2, 8+cold);
            break;
        }
        case STRIP_SCENES: color(i, 12, 18, 30); break;
        case STRIP_WORKING: color(i, 0, level, level+4); break;
        case STRIP_APPROVAL: color(i, level+8, level/2, 0); break;
        case STRIP_OFFLINE: color(i, level/3, level/3, level); break;
        case STRIP_LOW_BATTERY: color(i, 2, 0, 0); break;
        case STRIP_IDLE: color(i, 0, level/2, level); break;
        case STRIP_OFF: break;
        }
    }
    if (current == STRIP_WORKING) {
        uint8_t head = (now / 350) % LED_COUNT;
        color(head, 4, 24, 30);
    }
    if (event_started && now - event_started < 650 && current != STRIP_OFF) {
        uint8_t pulse = now - event_started < 160 ||
                        (now - event_started > 300 && now - event_started < 460) ? 30 : 6;
        for (uint8_t i=0; i<LED_COUNT; ++i)
            color(i, current == STRIP_APPROVAL ? pulse : 0,
                     current == STRIP_APPROVAL ? pulse/2 : pulse, pulse/2);
    }
    strip.show();
}
