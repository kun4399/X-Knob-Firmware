#pragma once
#include "../Page.h"
#include "hal/hal.h"
#include "services/GestureFSM.h"
#include "services/SceneStore.h"

namespace Page {
class Face : public PageBase {
public:
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewWillAppear() override;
    void onViewDidDisappear() override;
    void onViewDidUnload() override;
private:
    enum Screen { NORMAL, BRIGHTNESS, TEMPERATURE, SCENES, LIGHT_OFF } screen_ = NORMAL;
    lv_obj_t *drawing_ = nullptr, *caption_ = nullptr, *number_ = nullptr, *number_unit_ = nullptr, *hint_ = nullptr;
    lv_obj_t *overlay_title_ = nullptr;
    lv_obj_t *off_icon_arc_ = nullptr, *off_icon_stem_ = nullptr;
    lv_obj_t *scene_title_ = nullptr, *scene_index_ = nullptr, *scene_name_ = nullptr;
    lv_obj_t *scene_stats_ = nullptr, *scene_dots_[SceneStore::count] = {};
    lv_obj_t *scene_arcs_[2] = {};
    lv_obj_t *temperature_knob_ = nullptr;
    lv_obj_t *temperature_segments_[7] = {}, *gauge_ring_ = nullptr;
    lv_group_t *group_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    GestureFSM gesture_;
    SceneStore scenes_;
    HAL::SmartledState light_ = {};
    HAL::CodexState codex_ = {};
    int brightness_ = 50, temperature_ = 4000, selected_scene_ = 0;
    int cct_detent_position_ = 0;
    int8_t control_motor_mode_ = -1;
    bool power_ = false, dirty_ = false, pending_ = false, failed_ = false;
    bool change_power_ = false, change_brightness_ = false, change_temperature_ = false;
    uint8_t face_id_ = 0;
    uint32_t changed_at_ = 0, sent_at_ = 0, overlay_at_ = 0, face_at_ = 0;
    uint32_t last_face_render_at_ = 0;
    uint32_t light_seen_at_ = 0, event_revision_ = 0, ignore_motor_until_ = 0;
    uint8_t retries_ = 0;
    char pending_id_[64] = {};
    void update();
    void handle(GestureFSM::Event event, uint32_t now);
    void send(uint32_t now);
    void sync_control_motor_mode(uint32_t now);
    void choose_face(uint32_t now);
    void render_labels();
    void draw(const lv_area_t *clip);
    static void timer_cb(lv_timer_t *t);
    static void draw_cb(lv_event_t *e);
};
}
