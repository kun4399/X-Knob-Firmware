#include "services/LightControl.h"
#include <cassert>

int main() {
    assert(light_control_can_adjust(true, true));
    assert(!light_control_can_adjust(true, false));
    assert(!light_control_can_adjust(false, true));
    assert(!light_control_can_adjust(false, false));
    assert(light_control_should_show_off_ui(true, false, true));
    assert(!light_control_should_show_off_ui(true, false, false)); // off alone is not a popup
    assert(!light_control_should_show_off_ui(true, true, true));
    assert(!light_control_should_show_off_ui(false, false, true));
    assert(!light_control_off_ui_expired(false, 9999, 0));
    assert(!light_control_off_ui_expired(true, 1799, 0));
    assert(light_control_off_ui_expired(true, 1800, 0));
    assert(light_control_off_ui_expired(true, 0x6F8, 0xFFFFFFF0));
    int value = 50;
    value = light_control_apply_delta(value, 80, 1, 100);
    assert(value == 100);
    value = light_control_apply_delta(value, 1, 1, 100);
    assert(value == 100);
    value = light_control_apply_delta(value, -1, 1, 100);
    assert(value == 99); // no hidden encoder overshoot after saturation

    value = 1;
    value = light_control_apply_delta(value, -20, 1, 100);
    assert(value == 1);
    value = light_control_apply_delta(value, 1, 1, 100);
    assert(value == 2);

    int temperature = 6400;
    temperature = light_control_apply_delta(temperature, 400, 2700, 6500);
    assert(temperature == 6500);
    temperature = light_control_apply_delta(temperature, -100, 2700, 6500);
    assert(temperature == 6400);

    // A click cycles only the selected control page; none of these transitions
    // should itself mutate a light value or power state.
    assert(light_control_next_view(0) == 1); // brightness -> temperature
    assert(light_control_next_view(1) == 2); // temperature -> power
    assert(light_control_next_view(2) == 0); // power -> brightness
    assert(light_control_next_view(3) == 0); // invalid/scene index is fail-safe

    assert(light_control_power_for_delta(false, 1));
    assert(light_control_power_for_delta(true, 1));  // clockwise is idempotently ON
    assert(!light_control_power_for_delta(true, -1));
    assert(!light_control_power_for_delta(false, -1)); // counterclockwise is OFF
    assert(light_control_power_for_delta(true, 0));

    // The 5-degree app/editor detent moves about 53 K and one full turn spans
    // the usable CCT range. The 2-degree held-rotation mode is finer.
    assert(light_control_apply_cct_detents(4200, 1, 72, 2700, 6500) == 4253);
    assert(light_control_apply_cct_detents(4200, -1, 72, 2700, 6500) == 4147);
    int cct = 2700;
    for (int i = 0; i < 72; ++i)
        cct = light_control_apply_cct_detents(cct, 1, 72, 2700, 6500);
    assert(cct == 6500);
    cct = 4200;
    assert(light_control_apply_cct_detents(
               light_control_apply_cct_detents(cct, 1, 72, 2700, 6500),
               -1, 72, 2700, 6500) == cct);
    assert(light_control_apply_cct_detents(4200, 1, 180, 2700, 6500) == 4221);
    cct = 2700;
    for (int i = 0; i < 180; ++i)
        cct = light_control_apply_cct_detents(cct, 1, 180, 2700, 6500);
    assert(cct == 6480);
    assert(light_control_apply_cct_detents(cct, 1, 180, 2700, 6500) == 6500);
    assert(light_control_cct_detent_position(2700, 72, 2700, 6500) == 0);
    assert(light_control_cct_detent_position(6500, 72, 2700, 6500) == 72);
    assert(light_control_brightness_detent_position(1) == 0);
    assert(light_control_brightness_detent_position(100) == 99);
    assert(light_control_brightness_detent_position(0) == 0);
    assert(light_control_brightness_detent_position(101) == 99);
    assert(kFaceCctDetentCount == 122);
    assert(light_control_face_cct_position(2700) == 0);
    assert(light_control_face_cct_position(6500) == 122);
    assert(light_control_face_cct_temperature(0) == 2700);
    assert(light_control_face_cct_temperature(122) == 6500);
    for (int position = 0; position <= kFaceCctDetentCount; ++position) {
        const int temperature = light_control_face_cct_temperature(position);
        assert(light_control_face_cct_position(temperature) == position);
    }
}
