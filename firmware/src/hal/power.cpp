#include "hal.h"
#include "port/display.h"
#include "services/PowerButtonGate.h"
#include "RGB.h"
#include <Preferences.h>
#include <atomic>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

namespace {
std::atomic<bool> off_requested{false};
// RTC survives deep sleep; NVS also preserves manual off across USB resets
// or loss of supply. No credentials or device identifiers are stored here.
RTC_DATA_ATTR bool manual_off = false;
constexpr int kHeldPins[] = {BATTERY_OFF_PIN, ON_OFF_PIN, TFT_BLK, MO1, MO2, MO3, 38};

bool save_off(bool off) {
    Preferences prefs;
    if (!prefs.begin("xk-power", false)) return false;
    const bool saved = prefs.putBool("off", off) == sizeof(bool);
    prefs.end();
    return saved;
}

void sleep_off() {
    // Keep the existing power latch enabled: this is explicit deep sleep,
    // not an unverified hard battery cut. Retain black/backlight/driver pins.
    for (int pin : kHeldPins) gpio_hold_en(static_cast<gpio_num_t>(pin));
    gpio_deep_sleep_hold_en();
    // The click that selected shutdown (or a rejected short wake) must end
    // before arming active-low wake. Require a stable release, not one sample.
    uint32_t released_at = millis();
    while (millis() - released_at < PowerButtonGate::kReleaseMs) {
        if (digitalRead(PUSH_BUTTON_PIN) == LOW) released_at = millis();
        delay(5);
    }
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    rtc_gpio_init(static_cast<gpio_num_t>(PUSH_BUTTON_PIN));
    rtc_gpio_set_direction(static_cast<gpio_num_t>(PUSH_BUTTON_PIN), RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(static_cast<gpio_num_t>(PUSH_BUTTON_PIN));
    rtc_gpio_pulldown_dis(static_cast<gpio_num_t>(PUSH_BUTTON_PIN));
    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(PUSH_BUTTON_PIN), 0);
    esp_deep_sleep_start();
}
}

void HAL::power_init() {
    // Release retained outputs only after setting their safe boot levels.
    pinMode(BATTERY_OFF_PIN, OUTPUT);
    digitalWrite(BATTERY_OFF_PIN, LOW);
    pinMode(ON_OFF_PIN, OUTPUT);
    digitalWrite(ON_OFF_PIN, HIGH);
    for (int pin : {TFT_BLK, MO1, MO2, MO3, 38}) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
    gpio_deep_sleep_hold_dis();
    for (int pin : kHeldPins) gpio_hold_dis(static_cast<gpio_num_t>(pin));
    rtc_gpio_deinit(static_cast<gpio_num_t>(PUSH_BUTTON_PIN));
    pinMode(PUSH_BUTTON_PIN, INPUT_PULLUP);

    Preferences prefs;
    if (prefs.begin("xk-power", true)) {
        manual_off = prefs.getBool("off", false) || manual_off;
        prefs.end();
    }
    if (!manual_off) return; // ordinary first boot retains its existing behavior

    PowerButtonGate gate;
    for (;;) {
        const auto result = gate.update(digitalRead(PUSH_BUTTON_PIN) == LOW, millis());
        if (result == PowerButtonGate::SLEEP) sleep_off();
        if (result == PowerButtonGate::START) {
            if (!save_off(false)) sleep_off(); // failed storage cannot unlock boot
            manual_off = false;
            return; // release already consumed, so no click/long press leaks to Face
        }
        delay(5);
    }
}

void HAL::power_off() {
    off_requested.store(true);
}

void HAL::power_update() {
    if (!off_requested.exchange(false)) return;
    // Persist first. If storage fails, keep the running UI usable rather than
    // entering a shutdown that could forget its long-press requirement.
    if (!save_off(true)) {
        Serial.println("Shutdown cancelled: cannot save power state");
        return;
    }
    manual_off = true;
    if (handleTaskLvgl) vTaskSuspend(handleTaskLvgl);
    lcd_power_off();
    // All FOC/PWM changes remain in the motor task. Retry a full queue and
    // await its published acknowledgement before deep sleep freezes outputs.
    while (!motor_is_sensor_only()) {
        motor_set_sensor_only(true);
        delay(10);
    }
    strip_set_budget(0);
    strip_start(millis() + 40); // loop is the sole NeoPixel writer; flush black now
    sleep_off();
}
