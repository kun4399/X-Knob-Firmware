# Hardware files

This directory keeps the project-specific hardware references together.

| Path | Contents |
| --- | --- |
| `bom/SuperDialV2_BOM.xlsx` | Original board bill of materials |
| `cad/models/` | Knob, base, adapter, rotor sleeve, and CNC STEP model |
| `media/` | Power schematic and board/reference photos |

The source board is based on the open Super Dial design. See
[hardware notes](../docs/hardware.md) for board variants, known pin mapping,
and the limits of the inherited power notes. Check the schematic for the exact
PCB revision before changing wiring, GPIOs, or sleep behavior.
