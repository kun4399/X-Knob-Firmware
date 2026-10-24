#pragma once
#include <stdint.h>

// Run before display, motor and networking start. A short wake pulse must
// never reach the normal UI; a qualified hold is consumed through release.
class PowerButtonGate {
public:
    enum Result { WAIT, SLEEP, START };
    static constexpr uint32_t kHoldMs = 1200;
    static constexpr uint32_t kReleaseMs = 30;

    Result update(bool pressed, uint32_t now) {
        if (!started_) {
            started_ = true;
            pressed_at_ = now;
            if (!pressed) return SLEEP;
        }
        if (!qualified_) {
            if (!pressed) return SLEEP;
            if (now - pressed_at_ >= kHoldMs) qualified_ = true;
        }
        if (qualified_) {
            if (pressed) released_ = false;
            else if (!released_) { released_ = true; released_at_ = now; }
            else if (now - released_at_ >= kReleaseMs) return START;
        }
        return WAIT;
    }
private:
    bool started_ = false, qualified_ = false, released_ = false;
    uint32_t pressed_at_ = 0, released_at_ = 0;
};
