# Repository guide

## Project boundaries

- `firmware/` is the ESP32-S3 PlatformIO project. Build it from that directory
  with `pio run`.
- `macos/companion/` owns the native menu-bar app and bundled Python engine.
  Build with `app/build_app.py` and run its Python/bundle tests there; do not
  put these sources or runtime output in the PlatformIO source tree.
  Preserve menu-only operation and clean child shutdown; do not revive the
  retired LaunchAgent alongside the app. Hook changes must preserve unrelated
  entries, remain reversible, and never edit Codex trust records.
- `hardware/` contains reference BOM, CAD, and images. `docs/` contains the
  maintained explanations and verification record.
- The ESP32-WROOM-32 light gateway is maintained in the separate SmartLED
  repository.

## Safety and evidence

- Keep Wi-Fi/MQTT credentials, device identifiers, BLE keys, Keychain data,
  board dumps, and raw captures out of Git. Use ignored local configuration.
- Do not flash or erase hardware unless the serial port and intended board
  have been confirmed in the current session.
- Label host tests, compiled firmware, and hardware observations separately.
  A successful build does not establish physical UI, motor, or network behavior.
- Preserve board-specific pin mapping and motor settings unless the target
  hardware revision is known.

## Documentation

Update the root README and the relevant page under `docs/` when the directory
layout, controls, network contract, setup flow, or verification status changes.
Retain useful hardware and upstream attribution notes when replacing old
project-specific instructions.
