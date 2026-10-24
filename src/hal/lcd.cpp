#include "hal.h"
#include "app/Accounts/Account_Master.h"
#include "hal/motor.h"
#include "hal/nvs.h"
#include "RGB.h"
#include <Preferences.h>

namespace {
constexpr uint32_t TASK_PERIOD_MS = 20;
constexpr uint16_t DEFAULT_DIM_SECONDS = 45;
constexpr uint16_t DEFAULT_STANDBY_SECONDS = 180;
// Battery-only deep sleep remains disabled until this board's battery/VBUS
// sensing pins and current draw are verified on hardware.
enum DisplayState { AWAKE, DIM, STANDBY };
TaskHandle_t task_handle = nullptr;
Account *account = nullptr;
volatile int motor_position = 0;
bool motor_endstop_contact = false;
portMUX_TYPE power_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t last_activity_ms = 0;
bool wake_press_pending = false;
HAL::PowerSettings current_settings = {
    DEFAULT_DIM_SECONDS, DEFAULT_STANDBY_SECONDS, true
};

bool allowed(uint16_t value, const uint16_t *choices, size_t count) {
    for (size_t i = 0; i < count; ++i) if (choices[i] == value) return true;
    return false;
}

int on_motor_event(Account *, Account::EventParam_t *param) {
    auto *info = static_cast<MotorStatusInfo *>(param->data_p);
    portENTER_CRITICAL(&power_mux);
    motor_position = info->position;
    motor_endstop_contact = info->is_outbound;
    portEXIT_CRITICAL(&power_mux);
    return 0;
}

void brightness_task(void *) {
    bool previous_button = HAL::encoder_is_pushed();
    int previous_position = motor_position;
    DisplayState state = AWAKE;
    uint32_t activity = millis();
    for (;;) {
        uint32_t now = millis();
        bool button = HAL::encoder_is_pushed();
        int position = motor_position;
        bool active = button || button != previous_button || position != previous_position;
        portENTER_CRITICAL(&power_mux);
        if (last_activity_ms != 0 && (int32_t)(last_activity_ms - activity) > 0) {
            activity = last_activity_ms;
            active = true;
        }
        if (button && state == STANDBY) wake_press_pending = true;
        portEXIT_CRITICAL(&power_mux);
        if (active) activity = now;
        HAL::PowerSettings settings = {};
        HAL::power_settings_get(&settings);
        const uint32_t dim_after_ms = (uint32_t)settings.dim_after_seconds * 1000;
        const uint32_t standby_after_ms = (uint32_t)settings.standby_after_seconds * 1000;
        DisplayState next = active ? AWAKE :
            now - activity >= standby_after_ms ? STANDBY :
            now - activity >= dim_after_ms ? DIM : AWAKE;
        if (next != state) {
            state = next;
            int normal = get_lcd_bk_brightness();
            HAL::set_lcd_brightness(state == AWAKE ? normal :
                                    state == DIM ? (normal < 12 ? normal : 12) : 0);
            strip_set_budget(state == AWAKE ? 255 : state == DIM ? 96 : 30);
        }
        previous_button = button;
        previous_position = position;
        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}
}

void HAL::lcd_brightness_init() {
    static const uint16_t dim_choices[] = {15, 30, 45, 60};
    static const uint16_t standby_choices[] = {120, 180, 300, 600};
    Preferences prefs;
    if (prefs.begin("xk-power", false)) {
        PowerSettings loaded = {
            prefs.getUShort("dim_s", DEFAULT_DIM_SECONDS),
            prefs.getUShort("standby_s", DEFAULT_STANDBY_SECONDS),
            prefs.getBool("notify", true)
        };
        prefs.end();
        if (allowed(loaded.dim_after_seconds, dim_choices, sizeof(dim_choices) / sizeof(dim_choices[0])) &&
            allowed(loaded.standby_after_seconds, standby_choices, sizeof(standby_choices) / sizeof(standby_choices[0])) &&
            loaded.standby_after_seconds > loaded.dim_after_seconds) {
            portENTER_CRITICAL(&power_mux);
            current_settings = loaded;
            portEXIT_CRITICAL(&power_mux);
        }
    }
    ledcSetup(0, 5000, 10);
    ledcAttachPin(TFT_BLK, 0);
    account = new Account("LcdBrightness", AccountSystem::Broker(), 0, nullptr);
    account->SetEventCallback(on_motor_event);
    account->Subscribe("MotorStatus");
    xTaskCreatePinnedToCore(brightness_task, "LcdBkThread", 4096, nullptr, 2,
                            &task_handle, ESP32_RUNNING_CORE);
}

void HAL::power_note_activity() {
    portENTER_CRITICAL(&power_mux);
    last_activity_ms = millis();
    portEXIT_CRITICAL(&power_mux);
}

bool HAL::take_wake_press() {
    portENTER_CRITICAL(&power_mux);
    bool pending = wake_press_pending;
    wake_press_pending = false;
    portEXIT_CRITICAL(&power_mux);
    return pending;
}

bool HAL::motor_endstop_active() {
    portENTER_CRITICAL(&power_mux);
    bool active = motor_endstop_contact;
    portEXIT_CRITICAL(&power_mux);
    return active;
}

bool HAL::power_settings_get(PowerSettings *out) {
    if (!out) return false;
    portENTER_CRITICAL(&power_mux);
    *out = current_settings;
    portEXIT_CRITICAL(&power_mux);
    return true;
}

bool HAL::power_settings_set(const PowerSettings *settings) {
    static const uint16_t dim_choices[] = {15, 30, 45, 60};
    static const uint16_t standby_choices[] = {120, 180, 300, 600};
    if (!settings ||
        !allowed(settings->dim_after_seconds, dim_choices, sizeof(dim_choices) / sizeof(dim_choices[0])) ||
        !allowed(settings->standby_after_seconds, standby_choices, sizeof(standby_choices) / sizeof(standby_choices[0])) ||
        settings->standby_after_seconds <= settings->dim_after_seconds) return false;

    Preferences prefs;
    if (!prefs.begin("xk-power", false)) return false;
    bool saved = prefs.putUShort("dim_s", settings->dim_after_seconds) == sizeof(uint16_t) &&
                 prefs.putUShort("standby_s", settings->standby_after_seconds) == sizeof(uint16_t) &&
                 prefs.putBool("notify", settings->important_notifications);
    prefs.end();
    if (!saved) return false;
    portENTER_CRITICAL(&power_mux);
    current_settings = *settings;
    portEXIT_CRITICAL(&power_mux);
    power_note_activity();
    return true;
}

void HAL::set_lcd_brightness(int value) {
    if (value > 100) value = 100;
    if (value < 0) value = 0;
    ledcWrite(0, value * 10);
}
