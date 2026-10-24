#pragma once

#include "../Page.h"
#include "hal/hal.h"
#include "services/GestureFSM.h"
#include "services/SceneStore.h"

namespace Page {
class Smartled : public PageBase {
public:
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewDidLoad() override;
    void onViewWillAppear() override;
    void onViewDidAppear() override;
    void onViewWillDisappear() override;
    void onViewDidDisappear() override;

private:
    enum Mode { BRIGHTNESS, TEMPERATURE, POWER, SCENES } mode = BRIGHTNESS;
    lv_obj_t *ring = nullptr;
    lv_obj_t *temperature_segments[7] = {};
    lv_obj_t *temperature_knob = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *unit_label = nullptr;
    lv_obj_t *category_label = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_obj_t *scene_dots[SceneStore::count] = {};
    lv_timer_t *timer = nullptr;
    HAL::SmartledState state = {};
    GestureFSM gesture_;
    SceneStore scenes_;
    int desired_brightness = 50;
    int desired_temperature = 4000;
    int selected_scene = 0;
    bool desired_power = false;
    unsigned long last_state_ms = 0;
    unsigned long changed_ms = 0;
    unsigned long sent_ms = 0;
    unsigned long ignore_motor_until = 0;
    bool dirty = false;
    bool awaiting = false;
    bool failed = false;
    uint8_t retries = 0;
    Mode pending_mode = BRIGHTNESS;
    int pending_value = 0;
    bool pending_scene = false;
    bool scene_apply_dirty = false;
    bool pending_power = false;
    int pending_brightness = 0;
    int pending_temperature = 0;
    char pending_id[64] = {};

    void setMode(Mode next);
    void refresh();
    void update();
    void send();
    static void onTimer(lv_timer_t *timer);
};
}
