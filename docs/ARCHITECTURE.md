# Project structure and runtime architecture

This repository contains two separately run components: the ESP32-S3 firmware
and an optional macOS Codex-to-MQTT bridge. The Mac code is kept in
`mac_companion/`; it is not compiled into or uploaded with the knob firmware.
The ESP32-WROOM-32 light gateway is a separate project in the SmartLED
repository.

```text
1.Firmware/
├── src/                 ESP32-S3 firmware
│   ├── app/             LVGL pages, navigation, and application state
│   ├── hal/             motor, encoder, LCD, NVS, and MQTT services
│   ├── port/            LVGL display and input adapters
│   ├── services/        gesture and light-control state machines
│   └── web/             local Wi-Fi/MQTT setup and OTA handlers
├── lib/                 firmware support libraries (including LVGL)
├── design/v48/          source catalogue for generated face drawings
├── tools/               asset generators and host-side tests
├── docs/                setup, interaction, protocol, and verification notes
└── mac_companion/       independent Python service, hooks, and tests
```

## Firmware startup and task ownership

`src/main.cpp` initializes the HAL, NVS, LVGL, application pages, and runtime
tasks. The motor loop is independent of LVGL so display work cannot replace its
high-frequency control loop. The motor task is the only owner of motor writes;
the Arduino loop owns NeoPixel writes; LVGL objects are changed by the UI task.

The board pin map and task-core assignments are in `src/config.h`. Motor
parameters and GPIO assignments are hardware-specific. Do not copy them to a
different board without checking its schematic and sensor arrangement.

## Firmware modules

- `src/hal/` implements motor/encoder input, LCD backlight and saved power
  settings, NVS, and the Wi-Fi/MQTT worker.
- `src/port/` connects LVGL flush/input callbacks to the LCD and rotary input.
- `src/services/` contains UI-independent gesture and light-control logic.
- `src/app/Pages/` contains pages registered through `AppFactory.cpp` and
  installed by `app.cpp`. `Face` is the normal startup destination; the V4.8
  carousel opens the light, scene editor, Codex, notifications, power, and
  settings modules.
- `src/web/` handles local Wi-Fi/MQTT setup and OTA upload. Never log submitted
  passwords.
- `design/v48/` is source data; `tools/generate_face_data.py` produces the
  compact drawing table used by the firmware. GIFs and browser assets are not
  runtime firmware assets.
- `mac_companion/` has a separate Python dependency file, virtual environment,
  unit tests, local hook ingress, and optional LaunchAgent. It publishes only
  allowlisted Codex metadata to the existing broker.

## Page and gesture behavior

`PageManager` owns page lifetime and transitions. A page creates LVGL objects
in its load hooks and releases timers/groups in unload hooks. The current page
routes and button/rotary contract are maintained in
[INTERACTION_MAP.md](INTERACTION_MAP.md); use that as the behavior reference
when changing navigation. In particular, a short press in the light app only
changes the active control view. The power view changes the light only on
rotation. Previews are not commands, and a light value is confirmed only after
gateway acknowledgement/readback described in [MQTT.md](MQTT.md).

## Build and test by component

Build firmware from the repository root with `pio run`. Host-only firmware
logic tests and asset generation commands are listed in
[V48_RELEASE.md](V48_RELEASE.md). For the Mac bridge, create a separate venv
inside `mac_companion/`, install its own `requirements.txt`, then run
`python -m unittest discover -s tests -v` from that directory. Installing or
testing the Mac bridge does not flash the knob. See
[Mac Companion setup](../mac_companion/README.md).

Build success and host tests do not prove screen rendering, motor feel, MQTT
delivery, or light behavior on a physical device. Current evidence and open
hardware checks are listed in [V48_RELEASE.md](V48_RELEASE.md).
