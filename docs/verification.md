# Verification status

## Manual shutdown menu — 2026-10-24

Flashed after a private full-Flash backup; physical shutdown/wake acceptance
is still pending:

- Seventh application item **关机** requests explicit deep sleep; selecting it
  by rotation alone does not shut down or change the room light.
- Portable power-button tests cover short press rejection, the 1.2-second
  hold threshold, consumed release, contact bounce and clock rollover.
- Actual LVGL menu code is exercised with synthetic hardware for the seventh
  label/index and a single shutdown request after its click animation, without
  sending any light command. The 240×240 host preview was rendered and inspected.
- All seven portable C++ suites, font coverage (227 CJK characters), source
  ownership checks and the LVGL page harness pass. PlatformIO reports
  `[SUCCESS]`: RAM 85,648 / 327,680 bytes, program 1,442,453 / 3,342,336 bytes.
- Shutdown saves its NVS flag before stopping workers. Boot checks it before
  display, motor or networking startup; GPIO 5 EXT0 is the only sleep wake source.
  Persistence, GPIO retention and task shutdown are source/build evidence,
  not board measurements.
- The intended ESP32-S3 was identified on its current port; physical Flash
  was detected as 16 MiB. A full 16,777,216-byte private backup completed and
  its partition table matched the build exactly. PlatformIO upload completed
  with `[SUCCESS]`; bootloader, partition table, OTA boot data and application
  all passed written-data hash verification. The existing NVS partition was
  outside every erased/written range. This proves image deployment, not
  physical shutdown, long-press wake or battery current.
- Hardware acceptance remains: USB and battery shutdown, repeated short
  presses, rotation while off, long-hold release without UI side effects,
  reset/USB reinsertion preserving off, and sleep current. Earlier flashing
  and user feedback below apply to the preceding Always Ready image only.

## Always Ready v3 — 2026-10-24

The current source integrates the supplied Always Ready v3 specification into
both repositories. See the [implementation record and all 32 acceptance cases](always-ready.md)
and the [archived input specification](always-ready/implementation-spec-v3.md).

- Six portable C++ test programs, font coverage and source ownership checks pass.
- Real Face/SmartLED/LVGL page code passes synthetic hardware tests for
  idempotence, reversal, readback ordering, offline/dark cancellation, precision
  exit, scene preview and wake consumption. POWER and LIGHT OFF layouts were
  rendered at 240×240 and inspected. These are host renders, not screen photos.
- Companion: 15 unit tests pass, including quiet tool bursts, 120-second
  heartbeat, approval replay watermark and Hooks upgrades preserving other hooks.
- Gateway: C protocol/MIoT tests and 19 Python tests pass.
- ESP32-S3 PlatformIO and ESP32 ESP-IDF 6.0.1 builds pass. Exact sizes and
  reproduction commands are recorded in the implementation record.
- Both current images have been flashed after complete private backups and
  partition checks; every written segment passed digest verification. The user confirmed
  the revised switch feel, dark rotation without LCD wake, and press-only wake.
  Current, endurance and remaining stress scenarios await dedicated tests.
  After user feedback, the bounded single-snap home profile was built, page-tested
  and flashed again; all four segments passed digest verification.
- The current gateway produced fresh non-retained BLE readback, matching state
  and ACK for a same-value command (one 0.983s sample), and a duplicate ACK on
  repeat. This is not switch latency P95 or proof of knob gestures. The broker observer later saw
  16 knob commands with matching BLE readback, including nine power samples
  ranging from 0.974 to 1.241s; sample P95 was 1.241s. This excludes physical
  input processing and does not yet meet the initial 1s target.
- After explicit user review, the proposed Hooks update was applied atomically
  with a private backup and unrelated hooks preserved. The existing Companion
  was restarted and healthz passed. No new autostart service was added.

## Earlier firmware evidence

Earlier project notes recorded successful firmware builds, private full-Flash
backups and confirmed image writes, an observed same-value gateway V1 command
with BLE readback, and user feedback about selected physical screen layouts.
These observations apply to those earlier images. They do not verify the new
SENSOR_ONLY path, dark input, revised gestures or battery target.

The V4.8 catalogue retains 32 expressions. The round Chinese font is generated
from LXGW WenKai Screen under SIL OFL 1.1; its license remains in
`firmware/tools/licenses/`. Current 12/20/28px subsets cover all 228 CJK
characters found by the source coverage check. Firmware builds use checked-in
font data, with no runtime font generation requirement.

## Flash and rollback

Identify and confirm each current serial port and intended board before any
flash or reset. The knob and WROOM gateway have different targets and images.
Keep a private full-Flash backup outside Git; NVS contains credentials and
identifiers. Do not infer Flash capacity solely from the generic PlatformIO
board name. Use the intended hardware's verified capacity when backing up.

Uploading firmware does not automatically install Companion or hooks. Review
the proposed hooks before explicit installation, as described in
`macos/companion/README.md`. Never upload raw captures, private config or
credential-bearing images as evidence.

The user explicitly deferred battery current and three twelve-hour runs to later.
This round ends with software integration, deployment and basic user acceptance;
no twelve-hour endurance result is claimed.

## macOS menu app — 2026-10-24

The standalone menu-bar app replaces the old LaunchAgent. Eight broken legacy
hooks were removed and six current hooks were migrated to the native helper.
Twenty Python tests and packaged-bundle privacy/install/runtime tests pass.
The installed app connected to the existing MQTT broker; configuration form
validation/reconnection and parent-exit cleanup were observed. Final GUI quit
and reopen clicking awaits an unlocked Mac. See [the app record](macos-app.md).
