# ESP32-S3 firmware

This directory is the PlatformIO project for the X-Knob controller. It owns
the LVGL interface, motor/encoder control, local setup page, saved scenes, and
optional MQTT clients.

## Build

```sh
cd firmware
pio run
```

The target is `esp32-s3-devkitc-1` with Arduino. For upload, select the
currently connected knob port explicitly after identifying it:

```sh
pio run -t upload --upload-port "$KNOB_PORT"
```

The project defaults and pin map are in `src/config.h`; PlatformIO options
are in `platformio.ini`. Do not put a machine-specific port or real MQTT host
identity in committed settings.

## Code map

- `src/app/` — LVGL pages and page navigation.
- `src/hal/` — motor, encoder, display, NVS, and MQTT hardware services.
- `src/services/` — portable gesture, light-control, navigation, and scene
  storage logic.
- `src/port/` — display and rotary-input adapters.
- `src/web/` — local network configuration and OTA handlers.
- `lib/` — project libraries, including vendored LVGL.
- `design/v48/` — face drawings and animation data sources.
- `tools/` — generators and host-side logic checks.

Project-wide setup and interaction details are at
[the repository root](../README.md) and in [the maintained docs](../docs/).
Keep `src/secrets.h`, build products, and raw runtime logs out of Git.
