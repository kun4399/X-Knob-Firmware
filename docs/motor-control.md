# Motor control and FOC code tour

The motor uses SimpleFOC with a BLDC motor and an MT6701 magnetic encoder. The
FOC loop runs in the motor task and is kept independent from LVGL rendering.
The implementation is in `firmware/src/hal/motor.cpp` and its mode/settings
definitions are in `firmware/src/hal/motor.h`.

## Reading order

1. `motor.h`: motor modes and `XKnobConfig` fields.
2. `motor.cpp`: sensor/driver initialization, FOC loop, velocity control, and
   torque output.
3. `firmware/src/services/`: the higher-level detent, bound, gesture, and
   menu-navigation behavior that selects motor modes.
4. `firmware/tools/test_*.cpp`: host tests for gesture and detent-scaled
   controls. These tests do not model physical torque or friction.

Useful parameters include `position_width_radians` (spacing between detents),
`detent_strength_unit` (centering strength), `endstop_strength_unit` (boundary
force), and `snap_point` (the threshold for moving to the next position).
Their effective feel also depends on motor, encoder, driver, and mechanical
assembly; tune them on the intended hardware.

SimpleFOC references:

- [SimpleFOC documentation](https://docs.simplefoc.com/)
- [SimpleFOC Studio](https://github.com/JorgeMaker/SimpleFOCStudio)

Do not copy motor settings between hardware variants without confirming the
motor, pole pairs, phase wiring, sensor direction, and power stage.
