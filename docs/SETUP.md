# First-time setup

## Build the firmware

Install PlatformIO. On first setup, copy `firmware/src/secrets.h.example` to
ignored `firmware/src/secrets.h`; preserve an existing private header. Build
from the firmware directory:

```sh
cd firmware
pio run
```

Check the PlatformIO environment in `firmware/platformio.ini`. For upload, select the intended board and serial port in PlatformIO or add the appropriate `upload_port` locally; do not commit machine-specific port settings.

## Configure the network

1. Start the knob and open its local Wi-Fi configuration flow as described by the device UI.
2. Enter a 2.4 GHz Wi-Fi network and MQTT broker host, port, username, password, and topic prefix.
3. Enter the same broker account and topic prefix in the SmartLED gateway.
4. Confirm the gateway publishes retained state on `<prefix>/smartled/strip/state` before opening the Smart LED page.

Network values are persisted in NVS. If the firmware has previously stored settings, changing compile-time defaults does not replace those values. Use the device configuration flow to update them.

For Codex status, set `XK_CODEX_HOST_ID` in the ignored
`firmware/src/secrets.h` to the same value as Companion's private `XKNOB_MAC_ID`,
then rebuild. `config.h` reads this override before its public placeholder.
Do not edit a real deployment ID into tracked configuration.

## Use the Smart LED page

The default Face page and dark screen use clockwise ON / counterclockwise OFF.
Repeated turns toward the same state are idempotent. Short press on Face opens
brightness adjustment when the light is on; another short press selects CCT.
Double-click from adjustment returns to Face; double-click on Face opens apps.
Long press opens scene preview. A press in DARK only wakes to Face and is
consumed. Rotation in DARK keeps the LCD off. These new interactions need
physical acceptance on the intended board.

- Short-press cycles the brightness, color-temperature, and power views without
  changing the light state.
- Rotate on the brightness or temperature view to change that value. On the
  power view, rotate clockwise to turn on and counterclockwise to turn off.
- Long-press to open scene preview. In scene preview, long-press returns to the
  app carousel with exit feedback. Double-click from the light page returns to
  the carousel silently.
- Wait for the status label to show the gateway confirmed the value.

If the display appears blank but the device still responds to input, first check the LCD backlight brightness in display settings; brightness `0` turns the backlight off.

Automatic defaults are 15 seconds to DIM and 45 seconds to DARK. Change them
in POWER; the deep-sleep row is a read-only explanation. Existing valid saved
timeout choices are preserved. USB and battery use the same policy because
VBUS sensing has not been verified. Do not infer a twelve-hour runtime from
the settings or a successful build.

## Privacy

Do not share `firmware/src/secrets.h`, NVS dumps, raw serial output, firmware images built with private keys, or screenshots of configuration forms. The firmware must never print submitted Wi-Fi or MQTT passwords.
