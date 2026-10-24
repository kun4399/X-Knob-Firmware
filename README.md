# X-Knob

X-Knob is a personal rotary-control project built around an ESP32-S3, a round
240×240 LCD, and a haptic BLDC knob. The firmware combines the electronic-face
interface, light control through an ESP32-WROOM gateway, local scenes, and an
optional macOS Companion that forwards allowlisted Codex status to MQTT.

Always Ready v3 makes clockwise rotation an absolute ON command and
counterclockwise rotation an absolute OFF command on the face and while the
screen is dark. The lit home page has two switch endpoints with a single snap
between OFF and ON, as requested after hardware feedback. A short press opens precision adjustment; it consumes the
press when waking the screen. Defaults are 15 seconds to dim and 45 seconds
to dark, with Wi-Fi/MQTT and rotation detection kept online. Twelve-hour
battery life is a measurement target, pending hardware tests. See the
[implementation and acceptance record](docs/always-ready.md).

The application carousel also has a **关机** (Shut down) item. Rotate to it
and click to enter manual deep sleep, with the screen, RGB and motor output
off and Wi-Fi/MQTT stopped. After shutdown, hold the knob for at least about
1.2 seconds and release to boot; short presses return to sleep without
starting the UI. This new shutdown path is compiled and host-tested, pending
hardware acceptance. Automatic DARK standby keeps its existing direct controls.

The project is maintained as one product with independent components:

```text
firmware/       ESP32-S3 PlatformIO firmware, LVGL UI, generators, host tests
macos/companion Native macOS menu-bar app, bundled MQTT engine, Codex hooks
hardware/       BOM, CAD files, and board reference images
docs/           Current setup, architecture, interaction, and test notes
```

## Build and test

Build the knob firmware:

```sh
cd firmware
# First setup only: copy src/secrets.h.example to src/secrets.h and edit locally.
# Preserve an existing private header.
pio run
```

Run firmware logic and font checks from `firmware/`:

```sh
cd firmware
sh tools/run_host_tests.sh
# Optional real LVGL/page logic with synthetic hardware and BMP previews:
python3 tools/host_ui/run.py
```

Run the optional Mac Companion tests:

```sh
cd macos/companion
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
.venv/bin/python -m unittest discover -s tests -v
```

The ESP32-WROOM light gateway is maintained in the separate
[SmartLED gateway repository](https://github.com/kun4399/Smartled-esp32).
Build success and host tests do not substitute for checking the screen, motor,
network, or light on the actual devices. See [verification status](docs/verification.md).

## Documentation

- [First-time setup](docs/SETUP.md)
- [System architecture and directory guide](docs/ARCHITECTURE.md)
- [Page and control behavior](docs/interactions.md)
- [Always Ready v3 implementation and acceptance](docs/always-ready.md)
- [MQTT protocol](docs/protocols/mqtt.md)
- [Hardware, power, and connector notes](docs/hardware.md)
- [Motor and FOC code tour](docs/motor-control.md)
- [Development notes and project origins](docs/development.md), [attribution](docs/origins.md)
- [Mac Companion setup](macos/companion/README.md)
- [Menu-bar app implementation and verification](docs/macos-app.md)

Hardware files are grouped under [hardware/](hardware/README.md). Runtime
credentials, local device identifiers, board dumps, serial captures, and build
outputs stay outside version control. See [AGENTS.md](AGENTS.md) for repository
working rules.
