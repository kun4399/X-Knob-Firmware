# Project origins and attribution

X-Knob is now maintained as its own project. Its earlier hardware and
firmware work drew on public projects and design ideas; this record preserves
those useful references without carrying forward the old project README as the
current product guide.

- [Super Dial motor knob hardware](https://oshwhub.com/45coll/a2fff3c71f5d4de2b899c64b152d3da5) — source hardware design and BOM context.
- [Super Dial firmware](https://gitee.com/coll45/super-dial-motor-knob-screen) — related firmware reference.
- [smartknob](https://github.com/scottbez1/smartknob) — haptic rotary-control concept.
- [X-TRACK](https://github.com/FASTSHIFT/X-TRACK) — page navigation and account-message patterns used by the inherited firmware.
- [Peak](https://github.com/peng-zhihui/Peak) — an earlier hardware/firmware reference.
- [LVGL](https://github.com/lvgl/lvgl) — display framework; its license is included with the vendored source under `firmware/lib/lvgl/`.
- [SimpleFOC](https://github.com/simplefoc/Arduino-FOC) — motor-control library.

The original README credited a Super Dial assembly video and used local
reference photos. The retained image files are in `hardware/media/`; preserve
their source attribution when redistributing them. Project license terms are
in the root [LICENSE.md](../LICENSE.md), and the generated WenKai font carries
its own OFL license in `firmware/tools/licenses/`.
