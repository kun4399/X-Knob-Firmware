# V4.8 firmware and Mac Companion release notes

## Architecture

The ESP32-S3 knob remains an Arduino/PlatformIO project with LVGL 8.1 and its
existing PageManager and settings pages. `Face` is the startup page. Its
double-click app selector is the six-module V4.8 carousel (light, scene edit,
Codex, notifications, power, settings), drawn for the 240 px round LCD; the old
vertical app list has been removed. The
catalogue under `firmware/design/v48/` is generated into a compact LVGL drawing
table with `python3 tools/generate_face_data.py`; no animated GIF is stored in the
firmware. The renderer contains all 32 expressions and keyframe poses.

The knob subscribes to both legacy light state and V1 light state/ACK topics.
Before the WROOM-32 gateway is upgraded, it uses the legacy absolute commands
and waits for matching state. After V1 state is seen, it sends JSON commands
with IDs and checks both ACK and BLE readback. A failed or stale response is
displayed as unconfirmed. The existing Smart LED menu page remains available.

The face page maps rotation to brightness, held rotation to color temperature,
short press to power, a 1.2 second hold to scene selection, and double click to
the app carousel. Brightness and CCT input is gated by a fresh online,
powered-on gateway readback; when the lamp is off, the motor locks in both
directions. Pushing against that lock shows the V4.8 LIGHT · OFF page without
sending an adjustment, then returns to the electronic face 1.8 seconds after
the last attempt. A disconnected gateway shows the offline
face and locks the rotary motor. Brightness uses 100 bounded detents and color
temperature uses 122 two-degree detents matching the 244-degree UI arc; off or offline state selects a locked
single-position motor mode. Their detents use stronger torque than the lighter
app carousel, which is unchanged. In the SmartLED app, short press only cycles the brightness,
temperature, and power views; rotation adjusts the active value (clockwise ON,
counterclockwise OFF on the power view). Short taps wait through a 420 ms double-click window, during
which rotary jitter is discarded; button bounce near a tap is also suppressed.
The long-hold event fires while the button is still down, so scene return and
its haptic happen immediately without waiting for release. Scene names use the
Chinese UI font and the selection page shows each preset's brightness and color
temperature. Scene selection only previews local presets until short press
applies one. The separate scene editor saves four presets in two CRC checked
NVS slots.

The motor task owns all motor writes, the Arduino LED loop owns all NeoPixel
writes, and the LVGL task owns the new page objects. Event LEDs and haptics are
keyed to a new Codex event revision. The screen dims after 45 seconds and
enters online standby after three minutes. Automatic battery-only deep sleep
is disabled because this board's battery and VBUS sensing GPIOs are not known.
The existing explicit power-off menu action remains.

## 240 px round-screen layout

The V4.8 status aura uses the browser prototype's cheek arcs at 146..206 and
-26..34 degrees. Both sweep toward each other. The caption, status and footer
labels use a narrower 150 px lane near the lower edge of the circle; central
readouts use at most 190..204 px. Long labels end with an ellipsis instead of
drawing across the circular bezel. The Codex page maps long internal phase
names to short on-screen words. Scene editing and usage details use compact
phrases.

The 20 px Chinese UI font contains 226 source-used Han glyphs plus basic ASCII from
[LXGW WenKai Screen](https://github.com/lxgw/LxgwWenKai-Screen) under SIL OFL 1.1.
The generated bitmap is in `firmware/src/app/Fonts/`; its license is
in `firmware/tools/licenses/LXGW-WenKai-Screen-OFL.txt`. Firmware builds use the checked
in bitmap and require no font tool. To regenerate it, install the source TTF
and run `python3 tools/generate_round_font.py`; the generator uses
`lv_font_conv@1.5.3` and strips local font paths from the generated source.
Node.js and the source TTF are only needed when regenerating the font, not when
building or running the firmware.

## Reproduce builds and tests

```sh
cd firmware
python3 tools/generate_face_data.py
c++ -std=c++17 -Isrc tools/test_gesture.cpp -o /tmp/xknob-gesture-test
/tmp/xknob-gesture-test
pio run
cd ../macos/companion
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

The Mac needs VS Code with PlatformIO IDE or PlatformIO Core for the knob,
ESP-IDF 6.x for the separate gateway, Python with a project venv for the
Companion, and the installed Codex CLI. macOS supplies `launchd`, Keychain,
and SSH. A separate Codex plugin or local MQTT broker is unnecessary.
Mosquitto command line clients are optional diagnostic tools.

## Host-rendered screen audit

The LVGL host renderer was used to inspect the app carousel, all three SmartLED
control views, scene preview, scene editor, Codex overview/usage/token views,
notifications, power settings, and system settings at 240×240. Text remains
inside the circular panel and the primary value or selection is readable in
each view. All seven status expressions and their bilateral aura were also
rendered; aura opacity was raised because the original alpha was too faint on
the small panel. The low-battery expression was forced only in the host
renderer: battery sensing is still unavailable on this board revision.

These are LVGL host renders using synthetic gateway and Codex values. They
verify layout and drawing, not the physical LCD colors, viewing angle, motor
feel, network behavior, or the board's low-battery trigger.
The reference prototype's saved QA report lists 26/26 checks as passing. Its
Playwright suite could not be rerun on this Mac because Playwright is absent,
and the script hardcodes `/usr/bin/chromium`; this does not affect the LVGL
firmware renderer audit above.

## Verification boundary

- Host gesture, menu navigation, light-control, and detent-scaled CCT tests:
  passed.
- Exit haptics use the original X-Knob paired pulse parameters (strength 2,
  2 ms per polarity) and are queued by the motor task; physical feel still
  requires user confirmation.
- Mac Companion unit tests: eight passed. Hook ingress, LaunchAgent health,
  SSH tunnel, NAS MQTT status/usage receipt, and topic ACL were checked. The
  service health event counter advanced during a real Codex turn; the broker
  retained `quality: observed`, `focused_phase: running` and observed account
  usage. This verifies status transport, not the visual behavior of every
  lifecycle state.
- Knob PlatformIO build: passed (RAM 32.3%, Flash 68.5%). The ESP32-S3 image
  with the lighter 1.8 menu detent and calibrated CCT movement was flashed to
  the confirmed X-Knob port; the write and final readback reported matching
  digests. CCT steps now use each motor mode's
  physical detent count to traverse the usable temperature range in roughly one
  full turn. A private 16 MB pre-upgrade Flash backup is kept outside Git.
  Physical boot, CCT feel, and menu tactile feel of the latest image remain
  unconfirmed. On earlier images, the user observed side-positioned working
  auras, a legible 20 px caption, and no obvious clipping on pages inspected.
- Gateway ESP-IDF build and host protocol tests: passed. The confirmed 4 MB
  WROOM-32 was backed up privately, flashed, and image hash verified. A live
  same-value V1 command received a successful BLE-readback ACK. Duplicate ID,
  invalid value, and retained legacy state were checked through NAS MQTT.
- Knob gestures controlling the physical light, all 32 expressions and seven
  states, 8 LED direction, haptic current, sleep current, 48 hour endurance,
  and 100 wake cycles still require physical observation and measurement.
  Build success and one live MQTT command are not full hardware proof.

## Flash and rollback

Check that the selected serial port is the intended ESP32-S3 X-Knob before
uploading. Keep a private full Flash backup outside the repository; it
contains NVS credentials and device identifiers. The corresponding command
is `esptool.py --port PORT read_flash 0x0 0x1000000 PRIVATE_BACKUP.bin` for
the 16 MB board. Restore that exact private image only to the same confirmed
board with `esptool.py --port PORT write_flash 0x0 PRIVATE_BACKUP.bin`.
Do not commit either image, serial captures, `secrets.h`, or private config.

The Mac service and hooks are reversible with the `--uninstall` commands in
`macos/companion/README.md`; the NAS account and ACL should be removed only
after verifying nothing still subscribes to its topics.
