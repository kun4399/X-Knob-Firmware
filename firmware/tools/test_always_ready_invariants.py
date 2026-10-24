#!/usr/bin/env python3
"""Source ownership checks complement, but do not replace, behavior/hardware tests."""
from pathlib import Path

FW = Path(__file__).resolve().parents[1]
SRC = FW / "src"
for relative, function in (
    ("Face/Face.cpp", "Face::update"), ("Smartled/Smartled.cpp", "Smartled::update"),
    ("Codex/Codex.cpp", "Codex::update"), ("Codex/V48Aux.cpp", "V48Aux::update"),
    ("Menu/Menu.cpp", "Menu::update"), ("SceneEdit/SceneEdit.cpp", "SceneEdit::update"),
):
    code = (SRC/"app/Pages"/relative).read_text()
    start = code.index(function)
    assert "HAL::power_is_dark()" in code[start:start+400], relative
    assert "HAL::power_home_requested()" in code[start:start+600], relative

lcd = (SRC/"hal/lcd.cpp").read_text()
assert "esp_deep_sleep_start(" not in lcd
assert "HAL::motor_set_sensor_only(true)" in lcd
assert "last_motor_mode_retry >= 500" in lcd
assert 'HAL::smartled_set(true, desired' in lcd
app = (SRC/"app/app.cpp").read_text()
assert 'manager.Push("Pages/Face")' in app
assert 'manager.Push("Pages/Startup")' not in app
indev = (SRC/"port/lv_port/lv_port_indev.cpp").read_text()
assert "HAL::power_is_dark()" in indev and "HAL::power_home_requested()" in indev
rgb = (FW/"lib/strips/RGB.cpp").read_text()
assert "if (!dark_cleared) { strip.clear(); strip.show();" in rgb
for path in SRC.rglob("*.cpp"):
    if path.name == "motor.cpp":
        continue
    code = path.read_text()
    assert "motor.move(" not in code and "motor.loopFOC(" not in code, path
print("PASS: dark ownership across 6 page handlers and legacy input; sole motor writer; no automatic deep sleep; one-shot RGB clear")
