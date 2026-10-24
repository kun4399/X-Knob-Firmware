#include "services/SceneEditorLogic.h"
#include <cassert>

int main() {
    assert(scene_editor_next_focus(0) == 1);
    assert(scene_editor_next_focus(1) == 2);
    assert(scene_editor_next_focus(2) == 0);
    assert(scene_editor_next_focus(-1) == 0);
    assert(scene_editor_apply_delta(0, 1, 4) == 1);
    assert(scene_editor_apply_delta(3, 1, 4) == 0);
    assert(scene_editor_apply_delta(0, -1, 4) == 3);
    assert(scene_editor_apply_delta(1, 0, 4) == 1);
    assert(scene_editor_deadline_reached(1699, 1700) == false);
    assert(scene_editor_deadline_reached(1700, 1700));
    assert(scene_editor_deadline_reached(10, 0xFFFFFFF0)); // wrap-safe timer
    assert(!scene_editor_save_due(false, 2000, 1000));
    assert(!scene_editor_save_due(true, 1699, 1700));
    assert(scene_editor_save_due(true, 1700, 1700));
    assert(scene_editor_save_due(true, 10, 0xFFFFFFF0));
}
