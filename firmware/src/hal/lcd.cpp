#include "hal.h"
#include "app/Accounts/Account_Master.h"
#include "hal/motor.h"
#include "hal/nvs.h"
#include "services/AlwaysReadyPolicy.h"
#include "services/LightControl.h"
#include "RGB.h"
#include <Preferences.h>
#include <string.h>

// Battery-first software policy. This is NOT automatic light/deep sleep: the
// ESP32-S3 remains powered, WiFi/MQTT connected, and SPI rotation sampled.
// Only a battery-side current measurement can establish 12-hour endurance.
namespace {
constexpr uint32_t TASK_PERIOD_MS = 20;
constexpr uint16_t DEFAULT_DIM_SECONDS = AlwaysReady::Budget::kDimSeconds;
constexpr uint16_t DEFAULT_STANDBY_SECONDS = AlwaysReady::Budget::kDarkSeconds;
constexpr uint16_t kDimChoices[] = {10, 15, 20, 30, 45, 60};
constexpr uint16_t kStandbyChoices[] = {45, 60, 120, 180, 300, 600};
enum DisplayState { AWAKE, DIM, DARK };
TaskHandle_t task_handle = nullptr;
Account *account = nullptr;
volatile int motor_position = 0;
volatile bool motor_endstop_contact = false;
portMUX_TYPE power_mux = portMUX_INITIALIZER_UNLOCKED;
uint32_t last_activity_ms = 0;
bool wake_press_pending = false;
bool dark_screen = false;
bool dim_screen = false;
bool return_face_requested = false;
HAL::PowerSettings current_settings = {DEFAULT_DIM_SECONDS, DEFAULT_STANDBY_SECONDS, true};

bool allowed(uint16_t value, const uint16_t *choices, size_t count) {
    for (size_t i=0; i<count; ++i) if (choices[i] == value) return true;
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
    int32_t previous_position = HAL::get_motor_position();
    uint32_t previous_mode_epoch = HAL::motor_mode_epoch();
    DisplayState state = AWAKE;
    uint32_t activity = millis();
    uint32_t led_burst_until = 0;
    uint32_t last_codex_event = 0;
    uint32_t last_motor_mode_retry = 0;
    AlwaysReady::DarkTurnGate dark_turn;
    AlwaysReady::AbsoluteSwitchTxn dark_txn;
    char dark_command_id[64] = {};
    bool dark_command_power = false;
    uint32_t dark_command_at = 0;
    for (;;) {
        const uint32_t now = millis();
        const bool button = HAL::encoder_is_pushed();
        const auto input = HAL::motor_input();
        const int32_t position = input.position;
        const uint32_t motor_epoch = input.epoch;
        bool active = false;
        if (motor_epoch != previous_mode_epoch) {
            dark_turn.reset(position, now); // mode changes are NOT user turns
            previous_mode_epoch = motor_epoch;
            previous_position = position;
        }

        if (state == DARK) {
            // A physical button press is the explicit UI wake. Rotating the
            // shaft keeps LCD black and sends an ABSOLUTE ON/OFF command.
            if (button) {
                portENTER_CRITICAL(&power_mux);
                wake_press_pending = true;
                portEXIT_CRITICAL(&power_mux);
                active = true;
                dark_turn.reset(position, now);
            } else {
                const auto turn = dark_turn.sample(button, position, now);
                HAL::SmartledState reported = {};
                const bool have_state = HAL::smartled_state(&reported);
                const bool fresh = have_state && reported.online;
                if (dark_command_id[0] && light_control_readback_for_command(
                        reported.modern, reported.online, reported.ok,
                        reported.last_command_id, dark_command_id,
                        reported.received_ms, dark_command_at) &&
                    reported.power == dark_command_power &&
                    static_cast<uint32_t>(now - dark_command_at) < 10000) {
                    dark_txn.observed(true, reported.power, now);
                    dark_command_id[0] = 0;
                    strip_set_state(STRIP_LIGHT_BRIGHTNESS, 30, 4000, 0);
                    strip_set_budget(24); // confirmation is short, not permanent
                    led_burst_until = now + 280;
                }
                if (dark_command_id[0] &&
                    (!fresh || (strcmp(reported.last_ack_id, dark_command_id) == 0 &&
                                !reported.last_ack_success) ||
                     static_cast<uint32_t>(now - dark_command_at) >= 7000)) {
                    dark_txn.reset();
                    dark_command_id[0] = 0;
                    strip_set_state(STRIP_OFFLINE, 0, 4000, 0);
                    strip_set_budget(16);
                    led_burst_until = now + 320;
                }
                if (dark_txn.should_send(turn, fresh, reported.power, now)) {
                    char command_id[64] = {};
                    const bool desired = turn == AlwaysReady::SWITCH_ON;
                    // Never report success until BLE gateway verified readback.
                    if (HAL::smartled_set(true, desired, false, 0,
                                          false, 0, command_id, sizeof(command_id))) {
                        dark_txn.sent(turn, now);
                        strlcpy(dark_command_id, command_id, sizeof(dark_command_id));
                        dark_command_power = desired;
                        dark_command_at = now;
                    } else {
                        // WiFi offline is not a reason to light the LCD.
                        strip_set_state(STRIP_OFFLINE, 0, 4000, 0);
                        strip_set_budget(16);
                        led_burst_until = now + 320;
                    }
                }
            }
        } else {
            // On lit pages, gesture interpretation belongs to the active LVGL
            // page. The low-power controller does not steal its detents.
            active = button || button != previous_button ||
                     position != previous_position;
        }

        portENTER_CRITICAL(&power_mux);
        if (last_activity_ms != 0 && static_cast<int32_t>(last_activity_ms - activity) > 0) {
            activity = last_activity_ms;
            if (state != DARK) active = true;
        }
        portEXIT_CRITICAL(&power_mux);
        if (active) activity = now;

        HAL::PowerSettings settings = {};
        HAL::power_settings_get(&settings);
        const uint32_t dim_ms = static_cast<uint32_t>(settings.dim_after_seconds) * 1000;
        const uint32_t dark_ms = static_cast<uint32_t>(settings.standby_after_seconds) * 1000;
        const DisplayState next = active ? AWAKE :
            now - activity >= dark_ms ? DARK :
            now - activity >= dim_ms ? DIM : AWAKE;
        if (next != state) {
            state = next;
            portENTER_CRITICAL(&power_mux);
            dim_screen = state == DIM;
            portEXIT_CRITICAL(&power_mux);
            const int normal = get_lcd_bk_brightness();
            HAL::set_lcd_brightness(state == AWAKE ? normal :
                                   state == DIM ? (normal < 12 ? normal : 12) : 0);
            if (state == DARK) {
                portENTER_CRITICAL(&power_mux);
                dark_screen = true;
                return_face_requested = true;
                portEXIT_CRITICAL(&power_mux);
                dark_turn.reset(position, now);
                dark_txn.reset();
                dark_command_id[0] = 0;
                strip_set_budget(0);
                HAL::motor_set_sensor_only(true);
            } else {
                portENTER_CRITICAL(&power_mux);
                dark_screen = false;
                if (wake_press_pending) return_face_requested = true;
                portEXIT_CRITICAL(&power_mux);
                HAL::motor_set_sensor_only(false);
                strip_set_budget(state == AWAKE ?
                    AlwaysReady::Budget::kActiveRgbScale :
                    AlwaysReady::Budget::kDimRgbScale);
            }
        }

        // Retried until the single-owner motor task has actually entered the
        // requested mode. A full queue must not silently defeat power saving.
        if (now - last_motor_mode_retry >= 500) {
            last_motor_mode_retry = now;
            const bool want_sensor_only = state == DARK;
            if (HAL::motor_is_sensor_only() != want_sensor_only)
                HAL::motor_set_sensor_only(want_sensor_only);
        }

        // Dark-mode Codex alert: a single low-power RGB hint, never screen
        // wake, repeated buzz, or an indefinite RGB animation.
        HAL::CodexState codex = {};
        if (HAL::codex_state(&codex) && codex.event_revision != last_codex_event) {
            last_codex_event = codex.event_revision;
            if (state == DARK && settings.important_notifications &&
                codex.source_online &&
                strcmp(codex.event_kind, "codex.approval.requested") == 0) {
                strip_set_state(STRIP_APPROVAL, 20, 4000, codex.event_revision);
                strip_set_budget(24);
                led_burst_until = now + 850;
            }
        }
        if (state == DARK && led_burst_until &&
            static_cast<int32_t>(now - led_burst_until) >= 0) {
            strip_set_budget(0);
            led_burst_until = 0;
        }
        previous_button = button;
        previous_position = position;
        vTaskDelay(pdMS_TO_TICKS(TASK_PERIOD_MS));
    }
}
}

void HAL::lcd_brightness_init() {
    strip_set_budget(AlwaysReady::Budget::kActiveRgbScale);
    Preferences prefs;
    if (prefs.begin("xk-power", false)) {
        PowerSettings loaded = {
            prefs.getUShort("dim_s", DEFAULT_DIM_SECONDS),
            prefs.getUShort("standby_s", DEFAULT_STANDBY_SECONDS),
            prefs.getBool("notify", true)
        };
        prefs.end();
        if (allowed(loaded.dim_after_seconds, kDimChoices,
                    sizeof(kDimChoices) / sizeof(kDimChoices[0])) &&
            allowed(loaded.standby_after_seconds, kStandbyChoices,
                    sizeof(kStandbyChoices) / sizeof(kStandbyChoices[0])) &&
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
    xTaskCreatePinnedToCore(brightness_task, "LcdBkThread", 6144, nullptr, 2,
                            &task_handle, ESP32_RUNNING_CORE);
}

void HAL::power_note_activity() {
    portENTER_CRITICAL(&power_mux);
    last_activity_ms = millis();
    portEXIT_CRITICAL(&power_mux);
}

void HAL::lcd_power_off() {
    // Stop the policy worker before it can restore brightness, RGB or FOC.
    if (task_handle) vTaskSuspend(task_handle);
    HAL::set_lcd_brightness(0);
}

bool HAL::take_wake_press() {
    portENTER_CRITICAL(&power_mux);
    bool pending = wake_press_pending;
    wake_press_pending = false;
    portEXIT_CRITICAL(&power_mux);
    return pending;
}

bool HAL::power_is_dark() {
    portENTER_CRITICAL(&power_mux);
    bool dark = dark_screen;
    portEXIT_CRITICAL(&power_mux);
    return dark;
}

bool HAL::power_is_dim() {
    portENTER_CRITICAL(&power_mux);
    const bool dim = dim_screen;
    portEXIT_CRITICAL(&power_mux);
    return dim;
}

bool HAL::power_home_requested() {
    portENTER_CRITICAL(&power_mux);
    bool requested = return_face_requested;
    portEXIT_CRITICAL(&power_mux);
    return requested;
}

void HAL::power_ack_home() {
    portENTER_CRITICAL(&power_mux);
    return_face_requested = false;
    portEXIT_CRITICAL(&power_mux);
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
    if (!settings ||
        !allowed(settings->dim_after_seconds, kDimChoices,
                 sizeof(kDimChoices) / sizeof(kDimChoices[0])) ||
        !allowed(settings->standby_after_seconds, kStandbyChoices,
                 sizeof(kStandbyChoices) / sizeof(kStandbyChoices[0])) ||
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
    else if (value < 0) value = 0;
    ledcWrite(0, value * 10);
}
