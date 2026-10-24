# First-time setup

## Build the firmware

Install PlatformIO and build from the firmware directory:

```sh
pio run
```

Check the PlatformIO environment in `platformio.ini`. For upload, select the intended board and serial port in PlatformIO or add the appropriate `upload_port` locally; do not commit machine-specific port settings.

## Configure the network

1. Start the knob and open its local Wi-Fi configuration flow as described by the device UI.
2. Enter a 2.4 GHz Wi-Fi network and MQTT broker host, port, username, password, and topic prefix.
3. Enter the same broker account and topic prefix in the SmartLED gateway.
4. Confirm the gateway publishes retained state on `<prefix>/smartled/strip/state` before opening the Smart LED page.

Network values are persisted in NVS. If the firmware has previously stored settings, changing compile-time defaults does not replace those values. Use the device configuration flow to update them.

## Use the Smart LED page

- Short-press cycles the brightness, color-temperature, and power views without
  changing the light state.
- Rotate on the brightness or temperature view to change that value. On the
  power view, rotate clockwise to turn on and counterclockwise to turn off.
- Long-press to open scene preview. In scene preview, long-press returns to the
  app carousel with exit feedback. Double-click from the light page returns to
  the carousel silently.
- Wait for the status label to show the gateway confirmed the value.

If the display appears blank but the device still responds to input, first check the LCD backlight brightness in display settings; brightness `0` turns the backlight off.

## Privacy

Do not share `src/secrets.h`, NVS dumps, raw serial output, firmware images built with private keys, or screenshots of configuration forms. The firmware must never print submitted Wi-Fi or MQTT passwords.
