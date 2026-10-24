#pragma once
#include "../Page.h"
#include "services/GestureFSM.h"
#include "services/SceneStore.h"

namespace Page {
class SceneEdit : public PageBase {
public:
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewWillAppear() override;
    void onViewDidDisappear() override;
    void onViewDidUnload() override;
private:
    lv_obj_t *title_ = nullptr, *scene_name_ = nullptr, *scene_card_ = nullptr;
    lv_obj_t *save_status_ = nullptr;
    lv_obj_t *rows_[2] = {}, *field_labels_[2] = {}, *value_labels_[2] = {};
    lv_group_t *group_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    GestureFSM gesture_;
    SceneStore scenes_;
    LightScene working_[SceneStore::count] = {};
    bool modified_[SceneStore::count] = {};
    bool save_failed_ = false;
    bool save_pending_ = false;
    uint32_t save_due_at_ = 0, saved_until_ = 0;
    int selected_ = 0, selected_field_ = 0, brightness_ = 50, temperature_ = 4000;
    void update();
    void render(uint32_t now);
    bool save_modified(uint32_t now);
    void set_focus(int focus);
    void select_scene(int delta);
    static void timer_cb(lv_timer_t *t);
};
}
