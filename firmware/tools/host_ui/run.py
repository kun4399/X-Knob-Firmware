#!/usr/bin/env python3
"""Build real LVGL/page code against synthetic hardware; never access a board.

Usage: python3 tools/host_ui/run.py [--output /tmp/xknob-ui-tests]
Requires system C/C++ compilers only. BMP previews contain synthetic state.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess
import tempfile

HERE = Path(__file__).resolve().parent
FW = HERE.parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    output = args.output or Path(tempfile.mkdtemp(prefix="xknob-ui-tests-"))
    output.mkdir(parents=True, exist_ok=True)
    common = ["-O0", "-w", "-DLV_CONF_INCLUDE_SIMPLE", "-I"+str(HERE/"include"),
              "-I"+str(FW/"lib/lvgl"), "-I"+str(FW/"src"),
              "-I"+str(FW/"lib/button")]
    sources = sorted((FW/"lib/lvgl/src").rglob("*.c"))
    sources += sorted((FW/"src/app/Fonts").glob("*.c"))
    sources += [FW/f"src/app/Resources/Font/font_bahnschrift_{n}.c" for n in (17,32,65)]
    sources += [FW/rel for rel in (
        "src/app/Pages/Face/Face.cpp", "src/app/Pages/Smartled/Smartled.cpp",
        "src/app/Pages/Codex/V48Aux.cpp", "src/services/SceneStore.cpp",
        "src/app/Pages/Menu/Menu.cpp",
        "src/app/Utils/PageManager/ResourceManager.cpp",
        "src/app/Utils/lv_ext/lv_obj_ext_func.cpp")]
    sources += [HERE/"test_pages.cpp"]
    with tempfile.TemporaryDirectory(prefix="xknob-ui-build-") as temp:
        def compile_one(pair):
            index, source = pair
            obj = Path(temp)/f"{index}.o"
            command = ["c++", "-std=c++17"] if source.suffix == ".cpp" else ["cc", "-std=c11"]
            subprocess.run(command+common+["-c", str(source), "-o", str(obj)], check=True)
            return obj
        with ThreadPoolExecutor(max_workers=4) as pool:
            objects = list(pool.map(compile_one, enumerate(sources)))
        binary = Path(temp)/"test-pages"
        subprocess.run(["c++", *map(str, objects), "-o", str(binary)], check=True)
        subprocess.run([str(binary), str(output)], check=True)
    print(f"Synthetic LVGL previews: {output}")


if __name__ == "__main__":
    main()
