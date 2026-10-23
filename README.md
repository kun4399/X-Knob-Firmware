# X-Knob Firmware

PlatformIO firmware for the X-Knob ESP32-S3 rotary controller. This project combines a round LVGL display, a BLDC motor with detent feedback, local configuration, and an optional MQTT client for controlling the SmartLED gateway.

## What it does

- Renders the startup screen, menu, settings, system information, and interaction pages.
- Reads the rotary encoder and button, then applies motor modes for smooth, bounded, or strong detent feedback.
- Stores display, Wi-Fi, and MQTT settings in ESP32 NVS.
- Provides a local web page for Wi-Fi and MQTT setup and firmware upload.
- When MQTT is enabled, opens the **Smart LED** page to adjust brightness and color temperature, toggle power, and show the gateway's confirmed state.

The MQTT page expects the companion gateway's interface documented in [docs/MQTT.md](docs/MQTT.md). MQTT is optional at build time; network credentials are configured on the device and must not be committed.

## Build

Install PlatformIO Core or the PlatformIO IDE extension, then from this directory run:

```sh
pio run
```

The build environment is `esp32-s3-devkitc-1` with Arduino. Upload only after checking the connected board and its configured serial port:

```sh
pio run -t upload
```

Build output lives under `.pio/` and is ignored by Git. To disable the MQTT client, set `-DXK_MQTT=0` in `platformio.ini`. Do not distribute firmware images built with private credentials or device-specific keys.

## Configure Wi-Fi and MQTT

At first boot, the firmware initializes NVS with empty network credentials and a generic topic prefix. Use the device's configuration interface to enter the local Wi-Fi and MQTT broker settings. The SmartLED gateway and knob must use the same broker and topic prefix. See [docs/SETUP.md](docs/SETUP.md).

If you choose to use compile-time defaults for development, copy `src/secrets.h.example` to the ignored `src/secrets.h`, fill it locally, and keep the real file out of Git. Values entered through the device interface are saved in NVS. Do not post screenshots or serial output that reveal credentials.

## Source layout

| Path | Responsibility |
| --- | --- |
| `src/main.cpp` | Firmware entry point and main task setup |
| `src/hal/` | Motor, encoder, LCD, power, NVS, and MQTT hardware services |
| `src/port/` | LVGL display/input adapter code |
| `src/app/Pages/` | Screen/page implementations; each page owns its UI and interaction logic |
| `src/app/Accounts/` | Shared application state and command notifications |
| `src/app/Utils/PageManager/` | Page lifecycle, navigation, and transitions |
| `src/web/` | Local setup page, file management, and OTA upload handlers |
| `lib/` | Project-local support libraries, including LVGL and BLE HID code |
| `docs/` | Setup, architecture, MQTT interface, and verification notes |

For a guided code tour, read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md). The original project also contains Surface Dial and Home Assistant pages; those remain separate from the SmartLED MQTT page.

## Security and privacy

`src/secrets.h`, NVS contents, Wi-Fi and MQTT passwords, board identifiers, flash images, and raw serial logs are private runtime data. They are excluded from the repository. The setup access point and web interface should only be used in a trusted environment; do not expose them to the public Internet.
