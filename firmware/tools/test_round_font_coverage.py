#!/usr/bin/env python3
"""Ensure every CJK character used by page/service source exists in UI fonts."""

from pathlib import Path
import re
import sys

from generate_round_font import FONT_SIZES, SOURCES


def source_characters() -> set[int]:
    text = "".join(path.read_text(encoding="utf-8") for path in SOURCES)
    return {ord(char) for char in re.findall(r"[\u3400-\u9fff]", text)}


def font_characters(path: Path) -> set[int]:
    text = path.read_text(encoding="utf-8")
    offsets: dict[str, list[int]] = {}
    for match in re.finditer(
        r"static const uint16_t (unicode_list_\d+)\[\] = \{(.*?)\};", text, re.S
    ):
        offsets[match.group(1)] = [
            int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", match.group(2))
        ]

    codepoints: set[int] = set()
    for entry in re.finditer(
        r"\.range_start\s*=\s*(\d+).*?\.unicode_list\s*=\s*(unicode_list_\d+|NULL)",
        text,
        re.S,
    ):
        if entry.group(2) == "NULL":
            continue
        codepoints.update(
            int(entry.group(1)) + offset for offset in offsets[entry.group(2)]
        )
    return codepoints


def main() -> int:
    required = source_characters()
    failures = []
    for size, path in FONT_SIZES.items():
        missing = sorted(required - font_characters(path))
        if missing:
            failures.append(
                f"{size}px font misses: " + "".join(chr(codepoint) for codepoint in missing)
            )
    if failures:
        print("\n".join(failures), file=sys.stderr)
        return 1
    print(f"All {len(required)} CJK UI characters exist in all three round fonts.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
