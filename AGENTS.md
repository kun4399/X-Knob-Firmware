# Repository guide

## Project boundaries

- `firmware/` is the ESP32-S3 PlatformIO project. Build it from that directory
  with `pio run`.
- `macos/companion/` is a separate Python/macOS service. Run its tests from
  that directory; do not put it in the PlatformIO source tree.
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
