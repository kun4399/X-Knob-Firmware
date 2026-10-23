#pragma once

#include "../Page.h"
#include "hal/hal.h"

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
    void onViewDidUnload() override;

private:
    enum Mode { BRIGHTNESS, TEMPERATURE, POWER } mode = BRIGHTNESS;
    lv_obj_t *ring = nullptr;
    lv_obj_t *title = nullptr;
    lv_obj_t *value_label = nullptr;
    lv_obj_t *unit_label = nullptr;
    lv_obj_t *status_label = nullptr;
    lv_group_t *group = nullptr;
    lv_timer_t *timer = nullptr;
    HAL::SmartledState state = {};
    int desired_brightness = 50;
    int desired_temperature = 4000;
    bool desired_power = false;
    int last_position = -1;
    unsigned long last_state_ms = 0;
    unsigned long changed_ms = 0;
    unsigned long sent_ms = 0;
    bool dirty = false;
    bool awaiting = false;
    bool failed = false;
    Mode pending_mode = BRIGHTNESS;
    int pending_value = 0;

    void setMode(Mode next);
    void refresh();
    void update();
    void send();
    static void onTimer(lv_timer_t *timer);
    static void onEvent(lv_event_t *event);
};
}
