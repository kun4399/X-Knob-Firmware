# X-Knob

X-Knob is a personal rotary-control project built around an ESP32-S3, a round
240×240 LCD, and a haptic BLDC knob. The firmware combines the electronic-face
interface, light control through an ESP32-WROOM gateway, local scenes, and an
optional macOS Companion that forwards allowlisted Codex status to MQTT.

The project is maintained as one product with independent components:

```text
firmware/       ESP32-S3 PlatformIO firmware, LVGL UI, generators, host tests
macos/companion Python service, Codex hooks, and macOS LaunchAgent
hardware/       BOM, CAD files, and board reference images
docs/           Current setup, architecture, interaction, and test notes
```

## Build and test

Build the knob firmware:

```sh
cd firmware
pio run
```

Run firmware logic and font checks from `firmware/`:

```sh
cd firmware
python3 tools/test_round_font_coverage.py
for test in gesture menu_navigation light_control scene_editor_logic; do
  c++ -std=c++17 -Isrc "tools/test_${test}.cpp" -o "/tmp/xknob-${test}-test"
  "/tmp/xknob-${test}-test"
done
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

- [First-time setup](docs/setup.md)
- [System architecture and directory guide](docs/architecture.md)
- [Page and control behavior](docs/interactions.md)
- [MQTT protocol](docs/protocols/mqtt.md)
- [Hardware, power, and connector notes](docs/hardware.md)
- [Motor and FOC code tour](docs/motor-control.md)
- [Development notes and project origins](docs/development.md), [attribution](docs/origins.md)
- [Mac Companion setup](macos/companion/README.md)

Hardware files are grouped under [hardware/](hardware/README.md). Runtime
credentials, local device identifiers, board dumps, serial captures, and build
outputs stay outside version control. See [AGENTS.md](AGENTS.md) for repository
working rules.
