# SmartLED MQTT contract

X-Knob and the companion ESP32 gateway connect to the same local broker. The knob sends commands; the gateway performs BLE operations and publishes the actual state read back from the light. Home Assistant is optional.

## Topics

Replace `<prefix>` with the same topic prefix configured on both devices. `home` is a generic example.

| Direction | Topic | Payload |
| --- | --- | --- |
| Knob → gateway | `home/smartled/strip/set` | `state`, `power 0`, `power 1`, `brightness 1..100`, `temperature 2700..6500` |
| Gateway → knob | `home/smartled/strip/state` | Retained JSON state |

V4.8 also subscribes to the gateway's versioned topics. Until the V1 gateway
has been flashed, the face sends legacy absolute commands and verifies the
matching readback. Once it sees V1 state, it uses these topics:

| Direction | Topic | Payload |
| --- | --- | --- |
| Knob → gateway | `xknob/v1/light/main/set` | Non-retained JSON `{v,command_id,desired}` |
| Gateway → knob | `xknob/v1/light/main/ack` | `{command_id,success,ack_stage}` |
| Gateway → knob | `xknob/v1/light/main/state` | Retained BLE readback with `last_command_id` |
| Gateway → knob | `xknob/v1/light/main/availability` | Retained `online`/`offline` |

`desired` can contain `power`, `brightness_percent`, and
`color_temperature_kelvin`. The knob confirms only an online, successful BLE
readback with the same command ID and matching requested values. State may
arrive before ACK and confirm immediately. A successful ACK alone cannot
confirm; a failed ACK marks the pending command as failed. Timeout, page exit,
dark transition and loss of connectivity cancel old UI intent; timeout never
replays it with a new ID. Reconnection drops queued output and requests actual
state. A new opposite direction remains actionable while a command is pending.

The gateway polls idle BLE state every 30 seconds; writes still trigger
immediate readback. The knob's light TTL is 90 seconds. Wi-Fi modem sleep is
enabled and MQTT keepalive is 60 seconds; neither radio nor angle sampling is
shut down automatically in DARK. Physical latency remains unmeasured.

Mac Companion publishes status, non-retained events, usage, and availability
under `xknob/v1/codex/<configured-host-id>/`. Set `XKNOB_MAC_ID` in its private
environment file to match `XK_CODEX_HOST_ID` in ignored `firmware/src/secrets.h`, and
restrict the MQTT ACL accordingly. Do not commit a real device or account ID. Its
MQTT account should have no permission to write light commands. Hooks send only
allowlisted metadata; `Stop` maps to “本轮结束”.

Companion status is published on semantic change or an important event, plus
a 120-second heartbeat. It contains `boot_id` and `event_seq`; live events
contain the same `boot_id` and monotonic `seq`. The first status after each
connection establishes an event high-water mark. Events before that baseline,
from an unknown boot, or at/below the last accepted sequence are silent. This
prevents retained historical approvals and QoS duplicates from replaying RGB
alerts. Upgrade Companion together with the knob; an older status without the
watermark remains displayable but cannot authorize live alert playback.
Status carries at most four session entries to fit the bounded parser; counts
still include all sessions. The source TTL is 240 seconds, allowing missed
heartbeats. Ordinary events never publish to light topics or wake the LCD.

Example state:

```json
{"online":true,"ok":true,"power":true,"brightness":50,"temperature":4000}
```

`online` means the gateway currently has a readable BLE state. `ok` reports the last operation result. The knob waits for a state message matching its requested value and shows an error if the gateway does not confirm it before timeout. Commands are not retained; state is retained by the gateway.

The MQTT worker reconnects to Wi-Fi and the broker in the background. The UI requests the current state when it opens. If state becomes stale, the display reports the gateway as offline rather than treating an old preview as current.

## Local configuration

Configure broker host, port, username/password, Wi-Fi credentials, and topic prefix through the device setup page. Values persist in NVS. If you use compile-time first-boot defaults, keep the real `firmware/src/secrets.h` ignored and private. Build and gateway setup details are maintained in the separate SmartLED repository.

Use a broker account with access only to the required light and Codex topics.
Keep the broker on a trusted LAN or use a protected tunnel; the device setup
access point and HTTP form are not intended for Internet exposure.
