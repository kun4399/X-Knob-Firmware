#pragma once
#include <stdint.h>
#include <string.h>

// ACK is transport feedback. Only fresh, online BLE readback can confirm a
// desired value; V1 also requires the exact command ID, in either arrival order.
static inline bool light_control_readback_for_command(
    bool modern, bool online, bool ok, const char *readback_id,
    const char *command_id, uint32_t received_at, uint32_t sent_at) {
    if (!online || !ok || !received_at ||
        static_cast<int32_t>(received_at - sent_at) < 0) return false;
    return !modern || (command_id[0] && strcmp(readback_id, command_id) == 0);
}

#include <stdint.h>

// Apply a relative encoder delta without accumulating hidden overshoot at a
// software limit. The next detent away from the limit must change the value.
static inline int light_control_apply_delta(int current, int delta, int minimum, int maximum) {
    const long candidate = static_cast<long>(current) + delta;
    return candidate < minimum ? minimum : candidate > maximum ? maximum : static_cast<int>(candidate);
}

// Brightness and CCT are editable only when a fresh gateway readback says the
// strip is online and powered on. Local intent alone must never enable writes.
static inline bool light_control_can_adjust(bool online, bool power) {
    return online && power;
}

static constexpr uint32_t kLightOffUiDurationMs = 1800;
static constexpr int kFaceCctSweepDegrees = 244;
static constexpr int kFaceCctDetentDegrees = 2;
static constexpr int kFaceCctDetentCount =
    kFaceCctSweepDegrees / kFaceCctDetentDegrees;

static inline bool light_control_should_show_off_ui(bool online, bool power,
                                                    bool rotary_event) {
    return rotary_event && online && !power;
}

static inline bool light_control_off_ui_expired(bool showing, uint32_t now,
                                                uint32_t shown_at) {
    return showing && static_cast<int32_t>(now - shown_at) >=
                          static_cast<int32_t>(kLightOffUiDurationMs);
}

// Scale CCT changes to the physical detent spacing of the active motor mode.
// One full turn traverses the usable temperature range in either mode.
static inline int light_control_apply_cct_detents(int current, int delta,
                                                   int detents_per_revolution,
                                                   int minimum, int maximum) {
    if (detents_per_revolution <= 0 || maximum <= minimum) return current;
    const long numerator = static_cast<long>(delta) * (maximum - minimum);
    const long half = detents_per_revolution / 2;
    const long scaled = numerator >= 0
        ? (numerator + half) / detents_per_revolution
        : (numerator - half) / detents_per_revolution;
    return light_control_apply_delta(current, static_cast<int>(scaled), minimum, maximum);
}

static inline int light_control_cct_detent_position(int kelvin,
                                                     int detents_per_revolution,
                                                     int minimum, int maximum) {
    if (detents_per_revolution <= 0 || maximum <= minimum) return 0;
    const int bounded = light_control_apply_delta(kelvin, 0, minimum, maximum);
    const long numerator = static_cast<long>(bounded - minimum) * detents_per_revolution;
    return static_cast<int>((numerator + (maximum - minimum) / 2) /
                            (maximum - minimum));
}

static inline int light_control_face_cct_position(int kelvin) {
    return light_control_cct_detent_position(kelvin, kFaceCctDetentCount,
                                             2700, 6500);
}

static inline int light_control_face_cct_temperature(int position) {
    const int bounded = light_control_apply_delta(position, 0, 0,
                                                   kFaceCctDetentCount);
    const long numerator = static_cast<long>(bounded) * (6500 - 2700);
    return 2700 + static_cast<int>((numerator + kFaceCctDetentCount / 2) /
                                   kFaceCctDetentCount);
}

static inline int light_control_brightness_detent_position(int brightness) {
    return light_control_apply_delta(brightness, -1, 0, 99);
}

// SmartLED app short-click navigation. Values 0..2 are brightness,
// temperature, and power; scene preview is intentionally not part of the cycle.
static inline int light_control_next_view(int current) {
    return current == 0 ? 1 : current == 1 ? 2 : 0;
}

// The power view uses explicit direction rather than a toggle, so repeated
// clockwise/counterclockwise detents are idempotent and cannot invert state.
static inline bool light_control_power_for_delta(bool current, int delta) {
    return delta > 0 ? true : delta < 0 ? false : current;
}
