#include "SceneStore.h"
#include <Preferences.h>
#include <string.h>

namespace {
struct Record { uint32_t magic, revision; LightScene scenes[SceneStore::count]; uint32_t crc; };
constexpr uint32_t MAGIC = 0x53434E31;
int selected_scene = 0;
uint32_t crc32(const void *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = static_cast<const uint8_t *>(data);
    for (size_t i=0; i<len; ++i) {
        crc ^= p[i];
        for (int b=0; b<8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & -(crc & 1u));
    }
    return ~crc;
}
bool valid(const Record &r) {
    if (r.magic != MAGIC || r.crc != crc32(&r, sizeof(r)-sizeof(r.crc))) return false;
    for (auto &s: r.scenes)
        if (s.brightness < 1 || s.brightness > 100 || s.temperature < 2700 || s.temperature > 6500) return false;
    return true;
}
}

const char *SceneStore::name(int index) {
    static const char *NAMES[count] = {"阅读", "夜灯", "专注", "氛围"};
    return index >= 0 && index < count ? NAMES[index] : "SCENE";
}

int SceneStore::selected() { return selected_scene; }

bool SceneStore::select(int index) {
    if (index < 0 || index >= count) return false;
    selected_scene = index;
    return true;
}

void SceneStore::load() {
    Preferences prefs;
    // Create the namespace on first boot so an empty store is a clean default,
    // rather than a NOT_FOUND warning from a read-only open.
    if (!prefs.begin("xk-scenes", false)) return;
    Record a = {}, b = {};
    bool va = prefs.getBytesLength("slot0") == sizeof(a) && prefs.getBytes("slot0", &a, sizeof(a)) == sizeof(a) && valid(a);
    bool vb = prefs.getBytesLength("slot1") == sizeof(b) && prefs.getBytes("slot1", &b, sizeof(b)) == sizeof(b) && valid(b);
    prefs.end();
    const Record *best = va && (!vb || a.revision >= b.revision) ? &a : vb ? &b : nullptr;
    if (best) { memcpy(scenes_, best->scenes, sizeof(scenes_)); revision_ = best->revision; }
}

bool SceneStore::update(int index, int brightness, int temperature) {
    if (index < 0 || index >= count || brightness < 1 || brightness > 100 ||
        temperature < 2700 || temperature > 6500) return false;
    LightScene proposed[count];
    memcpy(proposed, scenes_, sizeof(proposed));
    proposed[index] = {static_cast<uint8_t>(brightness), static_cast<uint16_t>(temperature)};
    Record r = {};
    r.magic = MAGIC;
    r.revision = revision_ + 1;
    memcpy(r.scenes, proposed, sizeof(proposed));
    r.crc = crc32(&r, sizeof(r)-sizeof(r.crc));
    Preferences prefs;
    if (!prefs.begin("xk-scenes", false)) return false;
    const char *slot = (r.revision & 1) ? "slot1" : "slot0";
    bool saved = prefs.putBytes(slot, &r, sizeof(r)) == sizeof(r);
    prefs.end();
    if (saved) { memcpy(scenes_, proposed, sizeof(scenes_)); revision_ = r.revision; }
    return saved;
}
