#pragma once
#include <stdint.h>

struct LightScene { uint8_t brightness; uint16_t temperature; };

class SceneStore {
public:
    static constexpr int count = 4;
    static const char *name(int index);
    static int selected();
    static bool select(int index);
    void load();
    bool update(int index, int brightness, int temperature);
    LightScene get(int index) const { return scenes_[index]; }
private:
    LightScene scenes_[count] = {{90,5000}, {12,2700}, {75,4200}, {35,3000}};
    uint32_t revision_ = 0;
};
