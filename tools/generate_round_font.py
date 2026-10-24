#!/usr/bin/env python3
"""Regenerate the small 20 px Chinese UI font from an OFL font installed locally.

The generated C file is checked in, so building firmware needs no font tools.
Set XK_ROUND_FONT to an installed LXGW WenKai Screen TTF to regenerate it.
"""

import os
from pathlib import Path
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCES = sorted(
    [*ROOT.glob("src/app/Pages/**/*.cpp"), *ROOT.glob("src/app/Pages/**/*.h"),
     *ROOT.glob("src/services/**/*.cpp"), *ROOT.glob("src/services/**/*.h")]
)
FONT_SIZES = {
    12: ROOT / "src/app/Fonts/xknob_round_ui_12.c",
    20: ROOT / "src/app/Fonts/xknob_round_ui_20.c",
    28: ROOT / "src/app/Fonts/xknob_round_ui_28.c",
}


def main() -> None:
    font = os.environ.get("XK_ROUND_FONT")
    if not font:
        font = subprocess.check_output(
            ["fc-match", "-f", "%{file}", "LXGW WenKai Screen"], text=True
        ).strip()
    if not Path(font).is_file():
        raise SystemExit("Set XK_ROUND_FONT to the LXGW WenKai Screen TTF")
    chars = set()
    for source in SOURCES:
        chars.update(re.findall(r"[\u3400-\u9fff]", source.read_text()))
    next(iter(FONT_SIZES.values())).parent.mkdir(parents=True, exist_ok=True)
    for size, output in FONT_SIZES.items():
        subprocess.run(
            [
                "npx", "--yes", "lv_font_conv@1.5.3",
                "--no-compress", "--no-prefilter", "--bpp", "4", "--size", str(size),
                "--font", font, "-r", "0x20-0x7E", "--symbols", "".join(sorted(chars)),
                "--format", "lvgl", "-o", str(output), "--force-fast-kern-format",
            ],
            check=True,
        )
        generated = output.read_text()
        generated = re.sub(
            r"^ \* Opts:.*$",
            f" * Source: LXGW WenKai Screen (OFL 1.1); {size} px, 4 bpp, UI glyph subset.",
            generated,
            count=1,
            flags=re.MULTILINE,
        )
        generated = generated.replace(
            '#ifdef LV_LVGL_H_INCLUDE_SIMPLE\n#include "lvgl.h"\n#else\n'
            '#include "lvgl/lvgl.h"\n#endif',
            '#include "lvgl.h"',
            1,
        )
        output.write_text(generated.rstrip() + "\n")
        print(f"Generated {output.relative_to(ROOT)} with {len(chars)} Chinese glyphs")


if __name__ == "__main__":
    main()
