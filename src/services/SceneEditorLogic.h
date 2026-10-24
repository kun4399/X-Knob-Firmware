#pragma once
#include <stdint.h>

static constexpr uint32_t kSceneEditorSaveDebounceMs = 700;

static inline int scene_editor_apply_delta(int selected, int delta, int count) {
    if (count <= 0) return 0;
    const int step = delta % count;
    return (selected + step + count) % count;
}

static inline int scene_editor_next_focus(int focus) {
    return focus < 0 || focus >= 2 ? 0 : focus + 1;
}

static inline bool scene_editor_deadline_reached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

static inline bool scene_editor_save_due(bool pending, uint32_t now, uint32_t deadline) {
    return pending && scene_editor_deadline_reached(now, deadline);
}
