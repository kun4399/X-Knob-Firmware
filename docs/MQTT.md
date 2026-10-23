# SmartLED MQTT contract

X-Knob and the companion ESP32 gateway connect to the same local broker. The knob sends commands; the gateway performs BLE operations and publishes the actual state read back from the light. Home Assistant is optional.

## Topics

Replace `<prefix>` with the same topic prefix configured on both devices. `home` is a generic example.

| Direction | Topic | Payload |
| --- | --- | --- |
| Knob → gateway | `home/smartled/strip/set` | `state`, `power 0`, `power 1`, `brightness 1..100`, `temperature 2700..6500` |
| Gateway → knob | `home/smartled/strip/state` | Retained JSON state |

Example state:

```json
{"online":true,"ok":true,"power":true,"brightness":50,"temperature":4000}
```

`online` means the gateway currently has a readable BLE state. `ok` reports the last operation result. The knob waits for a state message matching its requested value and shows an error if the gateway does not confirm it before timeout. Commands are not retained; state is retained by the gateway.

The MQTT worker reconnects to Wi-Fi and the broker in the background. The UI requests the current state when it opens. If state becomes stale, the display reports the gateway as offline rather than treating an old preview as current.

## Local configuration

Configure broker host, port, username/password, Wi-Fi credentials, and topic prefix through the device setup page. Values persist in NVS. If you use compile-time first-boot defaults, keep the real `src/secrets.h` ignored and private. The gateway configuration and security notes are in the companion repository's setup guide.

Use a dedicated broker account with permissions limited to the two SmartLED topics. Keep the broker on a trusted LAN; the device setup access point and HTTP form are not intended for Internet exposure.
