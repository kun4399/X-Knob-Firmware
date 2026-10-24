# Architecture

The repository root is the X-Knob project. Each independently built component
has its own directory and instructions; only the ESP32-S3 project is part of
the PlatformIO build.

```text
firmware/
├── src/app/          LVGL pages, navigation, and shared application state
├── src/hal/          motor, encoder, LCD, NVS, and MQTT
├── src/port/         display and input adapters
├── src/services/     gesture, light-control, and scene-storage logic
├── src/web/          local network setup and OTA handlers
├── lib/              firmware libraries, including LVGL
├── design/v48/       source catalogues for generated face drawings
└── tools/             asset generators and host tests
macos/companion/       independent Python service and Codex hooks
hardware/              BOM, CAD, and visual references
docs/                  maintained project documentation
```

## Firmware runtime

`firmware/src/main.cpp` starts the hardware abstraction layer, NVS, LVGL,
application pages, and runtime tasks. The motor update loop runs independently
from LVGL so drawing work does not replace its high-frequency control loop.
The motor task is the sole owner of motor writes, the Arduino loop owns
NeoPixel writes, and the LVGL task owns page objects.

The backlight/power task owns AWAKE/DIM/DARK policy. In DARK it alone interprets
directional light input; all page gesture handlers and the legacy LVGL input
adapter are gated. PageManager returns to the permanent Face root on the LVGL
thread. The motor task disables drive output and samples the angle at about
100 Hz, publishing position and mode epoch together under a lock. It restores
the calibrated shaft angle and PID history before enabling FOC on wake.
There is no automatic deep sleep. See [Always Ready](always-ready.md).

Manual shutdown is a separate path. The seventh carousel item requests
`HAL::power_off()`; the Arduino loop persists manual off, suspends LVGL and
the brightness policy, waits for the motor owner's disabled-output
acknowledgement, clears RGB and enters deep sleep. Power pins retain their
existing latch levels. `power_init()` runs before UI allocation and motor
startup: it checks the NVS/RTC off marker and uses `PowerButtonGate` to reject
short presses or consume a 1.2-second hold through stable release. No automatic
standby timer calls this path. This feature still needs hardware acceptance.

The pin map and task-core assignments are in `firmware/src/config.h`. Motor
parameters and GPIO assignments are board-specific. Confirm the schematic
before adapting this firmware to a different PCB revision.

## Firmware modules

- `firmware/src/hal/` implements the encoder and motor, LCD/backlight, NVS,
  and Wi-Fi/MQTT worker.
- `firmware/src/port/` connects LVGL flush and input callbacks to the display
  and rotary input.
- `firmware/src/services/` contains UI-independent gesture, light-control,
  menu-navigation, and scene-persistence logic.
- `firmware/src/app/Pages/` contains the pages constructed by
  `AppFactory.cpp` and installed from `app.cpp`. The electronic face is the
  startup page; its carousel routes to light control, scene editing, Codex,
  notifications, power, settings, and a direct shutdown action. `Playground/` is retained as a shared
  rotary-control base used by the settings page; it is not a user-facing app.
- `firmware/src/web/` provides local Wi-Fi/MQTT setup and OTA handling. It must
  not log submitted credentials.
- `firmware/design/v48/` stores face-drawing source data. The generator in
  `firmware/tools/` emits compact firmware data; browser previews are not
  runtime assets.
- Firmware source no longer includes the unreachable Home Assistant and
  Surface Dial HID pages, the empty page template, the unused status bar, or
  the uncalled generated demo UI. LVGL remains vendored because it is a build
  dependency; its upstream examples are not used as application source.
- `macos/companion/app/` is the native AppKit menu-bar utility. It owns settings,
  Keychain access, hook management, and the lifetime of its bundled engine.
- `macos/companion/bridge/` preserves the Python MQTT/reducer/account adapters.
  PyInstaller packages the engine inside the app, so users need no external
  Python or checkout. Credentials are delivered via stdin rather than argv or
  config. The old LaunchAgent is retired on migration, and quitting the app
  closes the engine and SSH forward. Only allowlisted metadata is published.

## Page lifecycle and control behavior

`PageManager` owns page lifetime and transitions. Pages create LVGL objects in
their load hooks and release timers/groups in unload hooks. The complete
page-entry and gesture contract is in [interactions.md](interactions.md).
Light-control previews do not issue commands, and a value is confirmed only
after the matching gateway BLE readback documented in
[protocols/mqtt.md](protocols/mqtt.md).

## Build and verification

Build firmware from `firmware/` with `pio run`. Build and test the Mac service
from `macos/companion/` using its own Python virtual environment. The
[verification notes](verification.md) distinguish source checks, builds, host
tests, and behavior that still needs physical observation.
