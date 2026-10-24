# Hardware reference

## Main components

The inherited X-Knob hardware reference describes three boards (controller,
motor driver, and display), an ESP32-S3 WROOM-1U-N16R8, a 240×240 GC9A01 round
LCD behind 40 mm glass, an MT6701 magnetic encoder, a 3205 brushless motor, and
a 602535 600 mAh Li-ion battery. The V2 design notes list UPS support.

The project keeps the [BOM](../hardware/bom/SuperDialV2_BOM.xlsx), [CAD
models](../hardware/cad/models/), [power schematic](../hardware/media/power_sch.png),
and [board reference images](../hardware/media/).

## Firmware pin map

These values are the current firmware defaults in `firmware/src/config.h`.
They are not a substitute for checking the physical board revision.

| Function | Firmware pins |
| --- | --- |
| LCD SPI | SCLK 12, MOSI 11, CS 10, DC 14, reset 9, backlight 13 |
| Motor phases | 17, 16, 15 |
| MT6701 | SDA 1, SCL 2, chip select 42 |
| Push button | 5 |
| Power control | battery-off 7, on/off 18 |

## Recorded power behavior

The old hardware notes describe an MT2492 5 V-to-3.3 V regulator, an MT3609
battery boost stage, a TP4054 charger, and PMOS/NMOS-controlled power latching.
In that description, firmware holds `ON_OFF_PIN` high while running;
`BATTERY_OFF_PIN` low keeps the battery path enabled. Raising the battery-off
control releases the latch, while the push input wakes the ESP32 from deep
sleep and firmware reasserts the on/off control.

These are schematic/legacy notes, not a measurement of every board revision.
In particular, the current firmware does not enable automatic battery-only
deep sleep because battery/VBUS sensing pins have not been established for the
target board. See [verification status](verification.md) before relying on
sleep or battery behavior.

## Variant checks

- Confirm the PCB version and connector orientation before changing the pin
  map.
- Verify LCD rotation, motor phase order, encoder direction, and the battery
  power latch on the target unit.
- Do not infer battery or USB sensing from the existence of a battery power
  control signal; they are separate signals.
