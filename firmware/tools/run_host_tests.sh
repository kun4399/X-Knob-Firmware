#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
temp=$(mktemp -d "${TMPDIR:-/tmp}/xknob-host.XXXXXX")
trap 'rm -rf "$temp"' EXIT HUP INT TERM
for name in gesture always_ready command_readback light_control menu_navigation scene_editor_logic power_button; do
    c++ -std=c++17 -Wall -Wextra -Werror -I"$root/src" \
        "$root/tools/test_${name}.cpp" -o "$temp/$name"
    "$temp/$name"
    echo "PASS: $name"
done
python3 "$root/tools/test_round_font_coverage.py"
python3 "$root/tools/test_always_ready_invariants.py"
