# X-Knob Firmware

PlatformIO firmware for the X-Knob ESP32-S3 rotary controller. This project combines a round LVGL display, a BLDC motor with detent feedback, local configuration, and an optional MQTT client for controlling the SmartLED gateway.

## What it does

- Opens the V4.8 electronic face on startup, with 32 generated expression
  drawings, status arcs, light overlays, scenes, Codex status, menu, and settings.
- Reads the rotary encoder and button, then applies motor modes for smooth, bounded, or strong detent feedback.
- Stores display, Wi-Fi, and MQTT settings in ESP32 NVS.
- Provides a local web page for Wi-Fi and MQTT setup and firmware upload.
- When MQTT is enabled, controls the light from the face using readback
  confirmation. The older **Smart LED** page stays in the menu while the
  gateway migrates to the V1 JSON protocol.

The light interface is documented in [docs/MQTT.md](docs/MQTT.md), and the
implementation and verification boundary in [docs/V48_RELEASE.md](docs/V48_RELEASE.md).
The optional [Mac Companion](mac_companion/README.md) publishes Codex metadata
to the existing NAS broker. MQTT is optional at build time; network
credentials are configured on the device and must not be committed.

## Build

Install PlatformIO Core or the PlatformIO IDE extension, then from this directory run:

```sh
pio run
```

The build environment is `esp32-s3-devkitc-1` with Arduino. Upload only after checking the connected board and its configured serial port:

```sh
pio run -t upload --upload-port "$KNOB_PORT"
```

Set `KNOB_PORT` in your shell after checking which connected serial device is
the knob. Do not copy a remembered port from another board or session.

Build output lives under `.pio/` and is ignored by Git. To disable the MQTT client, set `-DXK_MQTT=0` in `platformio.ini`. Do not distribute firmware images built with private credentials or device-specific keys.

## Configure Wi-Fi and MQTT

At first boot, the firmware initializes NVS with empty network credentials and a generic topic prefix. Use the device's configuration interface to enter the local Wi-Fi and MQTT broker settings. The SmartLED gateway and knob must use the same broker and topic prefix. See [docs/SETUP.md](docs/SETUP.md).

If you choose to use compile-time defaults for development, copy `src/secrets.h.example` to the ignored `src/secrets.h`, fill it locally, and keep the real file out of Git. Values entered through the device interface are saved in NVS. Do not post screenshots or serial output that reveal credentials.
Set `XK_CODEX_HOST_ID` in `src/config.h` to the same local placeholder/host
ID configured as `XKNOB_MAC_ID` for the Mac Companion. Keep deployment IDs out
of commits.

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
| `design/v48/`, `tools/` | Source drawings, asset generators, and host-side tests |
| `mac_companion/` | Separate macOS Python service, Codex hooks, and its own tests |
| `docs/` | Setup, architecture, MQTT interface, and verification notes |

For a guided code tour, read [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
The Mac Companion has its own setup and test instructions in
[mac_companion/README.md](mac_companion/README.md); it is not part of the
firmware build. The original Surface Dial and Home Assistant pages remain
isolated from the V4.8 carousel and SmartLED control flow.

## Component checks

Run checks from the component's own directory:

```sh
# ESP32-S3 firmware
pio run

# Mac Companion (Python 3)
cd mac_companion
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

The ESP32-WROOM-32 gateway is built from its separate SmartLED repository.
See [docs/V48_RELEASE.md](docs/V48_RELEASE.md) for current evidence and the
boundary between host tests, builds, and device checks.

## Security and privacy

`src/secrets.h`, NVS contents, Wi-Fi and MQTT passwords, board identifiers, flash images, and raw serial logs are private runtime data. They are excluded from the repository. The setup access point and web interface should only be used in a trusted environment; do not expose them to the public Internet.
