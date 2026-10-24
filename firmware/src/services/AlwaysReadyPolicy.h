#pragma once
#include <stdint.h>

// Hardware-independent, allocation-free input policy for a battery-first X-Knob.
// The motor task owns the angle sensor. The power task consumes detent counts.
// No LVGL, MQTT, WiFi or SimpleFOC calls are made by this policy.
namespace AlwaysReady {

enum Intent : int8_t { NO_INTENT = 0, SWITCH_ON = 1, SWITCH_OFF = -1 };

class DarkTurnGate {
public:
    void reset(int32_t position, uint32_t now) {
        position_ = position;
        protected_until_ = now + kModeRebaseGuardMs;
        initialized_ = true;
        was_pressed_ = false;
    }

    Intent sample(bool pressed, int32_t position, uint32_t now) {
        if (!initialized_) { reset(position, now); return NO_INTENT; }
        const int32_t delta = position - position_;
        position_ = position;
        // Any press has priority, even before the button is debounced. The
        // mechanical displacement is dropped, not replayed on release.
        if (pressed) {
            was_pressed_ = true;
            protected_until_ = now + kAfterPressGuardMs;
            return NO_INTENT;
        }
        if (was_pressed_) {
            was_pressed_ = false;
            protected_until_ = now + kAfterPressGuardMs;
            return NO_INTENT;
        }
        if (static_cast<int32_t>(now - protected_until_) < 0) return NO_INTENT;
        return delta > 0 ? SWITCH_ON : delta < 0 ? SWITCH_OFF : NO_INTENT;
    }

private:
    static constexpr uint32_t kModeRebaseGuardMs = 80;
    static constexpr uint32_t kAfterPressGuardMs = 180;
    int32_t position_ = 0;
    uint32_t protected_until_ = 0;
    bool initialized_ = false;
    bool was_pressed_ = false;
};

class AbsoluteSwitchTxn {
public:
    // A flip is always actionable even if the currently reported state still
    // matches the newer intent: an older command may be in flight at the BLE gateway.
    bool should_send(Intent intent, bool reported_online, bool reported_power,
                     uint32_t now) {
        if (intent == NO_INTENT) return false;
        const bool target = intent == SWITCH_ON;
        const bool fresh_pending = in_flight_ &&
            static_cast<uint32_t>(now - sent_at_) < kPendingWindowMs;
        if (fresh_pending && pending_power_ == target) return false;
        if (in_flight_ && pending_power_ != target) return true;
        if (!fresh_pending && reported_online && reported_power == target) return false;
        return true;
    }

    void sent(Intent intent, uint32_t now) {
        in_flight_ = true;
        pending_power_ = intent == SWITCH_ON;
        sent_at_ = now;
    }

    void observed(bool verified, bool reported_power, uint32_t now) {
        // A readback associated with a current command should be checked by
        // the caller's command ID before marking its UI 'confirmed'. This
        // local hint solely suppresses redundant transport requests.
        if (in_flight_ && verified && reported_power == pending_power_ &&
            static_cast<uint32_t>(now - sent_at_) <= kPendingWindowMs)
            in_flight_ = false;
    }

    void reset() { in_flight_ = false; }
    bool pending() const { return in_flight_; }
    bool target_power() const { return pending_power_; }

private:
    static constexpr uint32_t kPendingWindowMs = 3500;
    bool in_flight_ = false;
    bool pending_power_ = false;
    uint32_t sent_at_ = 0;
};

struct Budget {
    // Software defaults. Real current MUST be measured; the target is <=40mA
    // average battery current for reserve beyond ideal 600mAh / 12h = 50mA.
    static constexpr uint16_t kDimSeconds = 15;
    static constexpr uint16_t kDarkSeconds = 45;
    static constexpr uint8_t kActiveRgbScale = 96;
    static constexpr uint8_t kDimRgbScale = 16;
    static constexpr uint16_t kSensorOnlyPollMs = 10; // ~100Hz
    static constexpr float kDarkStepDegrees = 12.0f;
};
}
