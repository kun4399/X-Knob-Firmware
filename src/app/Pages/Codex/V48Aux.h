#pragma once
#include "../Page.h"
#include "services/GestureFSM.h"
#include "hal/hal.h"

namespace Page {
class V48Aux : public PageBase {
public:
    enum Kind { NOTIFICATIONS, POWER, SETTINGS };
    explicit V48Aux(Kind kind) : kind_(kind) {}
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewWillAppear() override;
    void onViewDidDisappear() override;
    void onViewDidUnload() override;
private:
    Kind kind_;
    GestureFSM gesture_;
    lv_group_t *group_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    lv_obj_t *title_ = nullptr, *hero_ = nullptr, *detail_ = nullptr;
    lv_obj_t *rows_[4] = {};
    lv_obj_t *row_values_[4] = {};
    lv_obj_t *row_frames_[4] = {};
    lv_obj_t *dividers_[3] = {};
    lv_obj_t *mark_ = nullptr;
    HAL::PowerSettings edit_settings_ = {};
    uint8_t selected_row_ = 0;
    uint8_t edit_option_ = 0;
    bool editing_ = false;
    bool save_failed_ = false;
    uint32_t rendered_revision_ = 0xFFFFFFFFu;
    void update();
    void render();
    static void timer_cb(lv_timer_t *t);
};
}
