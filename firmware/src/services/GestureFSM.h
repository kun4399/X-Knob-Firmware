#pragma once
#include <stdint.h>

// Button/rotary arbitration shared by the face, app carousel, and editors.
// Position is the motor's detent counter, not a raw encoder pulse count.
class GestureFSM {
public:
    enum Kind { NONE, ROTATE, HOLD_ROTATE, SINGLE_CLICK, DOUBLE_CLICK, LONG_PRESS };
    struct Event { Kind kind; int delta; Event(Kind k = NONE, int d = 0) : kind(k), delta(d) {} };

    Event update(bool raw_pressed, int position, uint32_t now_ms) {
        if (!initialized_) {
            initialized_ = true;
            raw_ = stable_ = raw_pressed;
            raw_changed_ = now_ms;
            last_position_ = position;
            pressed_at_ = now_ms;
            return {};
        }

        if (raw_pressed != raw_) { raw_ = raw_pressed; raw_changed_ = now_ms; }
        if (raw_ != stable_ && now_ms - raw_changed_ >= kDebounceMs) {
            stable_ = raw_;
            last_position_ = position; // discard the mechanical click's small shaft movement
            if (stable_) {
                pressed_at_ = now_ms;
                moved_ = false;
                long_fired_ = false;
                second_press_ = pending_click_ && now_ms - click_at_ <= kDoubleGapMs;
                rotate_after_ = now_ms + kPressGuardMs;
            } else {
                uint32_t held = now_ms - pressed_at_;
                rotate_after_ = now_ms + kReleaseGuardMs;
                if (consume_wake_) {
                    consume_wake_ = false;
                    pending_click_ = false;
                } else if (second_press_) {
                    second_press_ = false;
                    if (!long_fired_ && !moved_) {
                        pending_click_ = false;
                        return {DOUBLE_CLICK, 0};
                    }
                    pending_click_ = false;
                } else if (!long_fired_ && !moved_ && held <= kDoubleGapMs) {
                    pending_click_ = true;
                    click_at_ = now_ms;
                }
            }
        }

        int delta = position - last_position_;
        last_position_ = position;
        if (consume_wake_) return {};
        if (pending_click_ && !stable_) {
            // Don't let button rebound or a tiny twist turn a click into a
            // brightness/CCT command while the double-click window is open.
            if (!raw_ && now_ms - click_at_ > kDoubleGapMs) {
                pending_click_ = false;
                return {SINGLE_CLICK, 0};
            }
            return {};
        }
        if (delta) {
            if ((int32_t)(now_ms - rotate_after_) >= 0) {
                moved_ = stable_;
                return {stable_ ? HOLD_ROTATE : ROTATE, delta};
            }
        }

        // Resolve rotation before a long press so a hold-and-turn event at
        // the threshold cannot be mistaken for a stationary long press.
        if (stable_ && !moved_ && !long_fired_ &&
            now_ms - pressed_at_ >= kLongPressMs) {
            long_fired_ = true;
            pending_click_ = false;
            // Fire while the button is still held. Pages may leave immediately;
            // parents must consume this same physical press through its release.
            consume_wake_ = true;
            return {LONG_PRESS, 0};
        }
        return {};
    }

    void consume_wake_press(bool pressed) {
        if (!pressed) return;
        consume_wake_ = true;
        pending_click_ = false;
        second_press_ = false;
    }
    // A page revealed by Pop may inherit the child's still-held long press.
    // Ignore that press through its release so the parent cannot synthesize a click.
    void suppress_pressed_until_release(bool pressed) {
        pending_click_ = false;
        second_press_ = false;
        consume_wake_ = pressed;
    }
    void rebase(int position) { last_position_ = position; }

private:
    static constexpr uint32_t kDebounceMs = 25;
    static constexpr uint32_t kDoubleGapMs = 420;
    static constexpr uint32_t kLongPressMs = 1200;
    static constexpr uint32_t kPressGuardMs = 110;
    static constexpr uint32_t kReleaseGuardMs = 80;
    bool initialized_ = false, raw_ = false, stable_ = false;
    bool moved_ = false, pending_click_ = false, consume_wake_ = false, long_fired_ = false, second_press_ = false;
    int last_position_ = 0;
    uint32_t raw_changed_ = 0, pressed_at_ = 0, click_at_ = 0, rotate_after_ = 0;
};
