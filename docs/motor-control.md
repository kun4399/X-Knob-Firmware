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

After user hardware feedback, the home page uses the existing bounded ON/OFF
profile: two endpoints, 60-degree spacing and one snap between OFF and ON.
Repeated turns beyond an endpoint do not create a continuous train of detents.
Pending intent determines the endpoint until readback arrives; external state
changes resynchronize it. This explicitly supersedes the reference overlay's
unbounded home detents. Precision/menu profiles remain separate. DARK switches the
same motor task to SENSOR_ONLY: it commands zero, disables the driver, skips
FOC, polls MT6701 at roughly 100 Hz, and counts 12-degree movement. Its sign
uses both the calibrated SimpleFOC sensor direction and `XK_INVERT_ROTATION`,
matching the awake coordinate system. Confirm physical clockwise ON on the
actual assembly. Mode requests use a queue and the power task retries the
requested sensor mode every 500 ms until acknowledged by the motor snapshot.

Wake refreshes the calibrated shaft angle, recenters the detent and resets
only PID history before enabling the driver. It does not rerun full FOC
calibration. The installed SimpleFOC 2.2.3 lacks `PIDController::reset()`, so
history is reconstructed with the existing gains. Drive power reduction,
absence of a wake jerk and sensor reliability across stress cases still require
hardware measurement. The user confirmed the basic switch feel and dark control;
a compiled disable/enable call does not prove current reduction.

SimpleFOC references:

- [SimpleFOC documentation](https://docs.simplefoc.com/)
- [SimpleFOC Studio](https://github.com/JorgeMaker/SimpleFOCStudio)

Do not copy motor settings between hardware variants without confirming the
motor, pole pairs, phase wiring, sensor direction, and power stage.
