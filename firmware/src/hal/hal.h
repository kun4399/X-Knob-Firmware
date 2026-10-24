#ifndef __HAL_H__
#define __HAL_H__
#include "button_event.h"
#include "HAL_Def.h"
#include <Arduino.h>
#include "config.h"
#include "CommonMacro.h"

typedef enum {
    SUPER_DIAL_NULL = 0,
    SUPER_DIAL_LEFT = 1,
    SUPER_DIAL_RIGHT= 2,
} SuperDialMotion;

namespace HAL
{
    struct SmartledState {
        bool online;
        bool ok;
        bool power;
        int brightness;
        int temperature;
        unsigned long received_ms;
        bool modern;
        char last_command_id[64];
        char last_ack_id[64];
        bool last_ack_success;
        unsigned long ack_received_ms;
    };
    struct CodexState {
        bool source_online;
        bool snapshot_valid;
        char phase[32];
        char quality[16];
        int running_count;
        int approval_count;
        unsigned long received_ms;
        uint32_t event_revision;
        char event_kind[48];
        bool usage_valid;
        int remaining_percent;
        uint8_t usage_window_count;
        int window_remaining_percent[2];
        int window_duration_minutes[2];
        uint64_t lifetime_tokens;
        uint64_t daily_tokens[7];
        uint8_t daily_token_count;
        uint8_t session_count;
        char session_phase[4][32];
        uint16_t session_age_seconds[4];
    };
    struct PowerSettings {
        uint16_t dim_after_seconds;
        uint16_t standby_after_seconds;
        bool important_notifications;
    };
    void Init();
    void Update();

    void knob_init();
    void knob_update(void);
    bool encoder_is_pushed(void);

    void motor_init(void);
    // void TaskMotorUpdate(void *pvParameters);
    int get_motor_position(void);
    bool motor_endstop_active(void);
    void update_motor_mode(int mode, int init_position);
    void motor_exit_feedback(void);

    void power_init(void);
    void power_off(void);

    bool is_encoder_enabled(void);
    void encoder_disable(void);
    void encoder_enable(void);

    void mqtt_init(void);
    int mqtt_publish(const char *topic, const char *playload);
    bool smartled_command(const char *command);
    bool smartled_set(bool include_power, bool power, bool include_brightness,
                      int brightness, bool include_temperature, int temperature,
                      char *command_id, size_t command_id_size);
    bool smartled_state(SmartledState *out);
    bool codex_state(CodexState *out);

    void lcd_brightness_init();
    void set_lcd_brightness(int value);
    void power_note_activity();
    bool take_wake_press();
    bool power_settings_get(PowerSettings *out);
    bool power_settings_set(const PowerSettings *settings);
}


#endif
