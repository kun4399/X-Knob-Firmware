# Development notes

## Round-screen UI assets

The firmware uses LVGL 8.1. The compact face catalogue lives in
`firmware/design/v48/`; regenerate its C drawing data with:

```sh
cd firmware
python3 tools/generate_face_data.py
```

The checked-in Chinese UI fonts are ready for firmware builds. Regenerating
them requires the licensed LXGW WenKai Screen source font and Node.js:

```sh
cd firmware
XK_ROUND_FONT=/path/to/LXGWWenKaiScreen.ttf python3 tools/generate_round_font.py
python3 tools/test_round_font_coverage.py
```

The generated source is path-scrubbed. The font license is preserved in
`firmware/tools/licenses/`.

## UI graphics and icons

New pages should use the shared round-screen drawing helpers in
`firmware/src/app/Pages/RoundUI.h` where practical. Keep core labels inside the
240×240 circular safe area. The historical icon design note used Iconoir
outlines at 42×42 px and 1.6 px stroke; treat it as an asset guide rather than
a dependency on the old asset directory.

## Shared application messages

Older pages use the X-TRACK-inspired Account system under
`firmware/src/app/Accounts/` and `firmware/src/app/Utils/AccountSystem/` for
publish/notify/pull messaging. New self-contained behavior belongs in
`firmware/src/services/`; retain the Account system where existing pages still
depend on it.

## Troubleshooting history

An old development note recorded a task watchdog reset while the LVGL task and
Arduino loop were active during MQTT connection attempts. The original note
did not isolate a cause; treat it as historical context, not a confirmed
current failure. If it recurs, capture a fresh redacted trace and identify the
blocking task before changing watchdog settings.
