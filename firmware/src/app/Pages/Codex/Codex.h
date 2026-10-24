#pragma once
#include "../Page.h"
#include "services/GestureFSM.h"

namespace Page {
class Codex : public PageBase {
public:
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewWillAppear() override;
    void onViewDidDisappear() override;
    void onViewDidUnload() override;
private:
    lv_obj_t *title_ = nullptr, *subtitle_ = nullptr, *value_ = nullptr, *unit_ = nullptr, *detail_ = nullptr;
    lv_obj_t *footer_ = nullptr, *divider_ = nullptr, *approval_dot_ = nullptr, *usage_arc_ = nullptr;
    lv_group_t *group_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    GestureFSM gesture_;
    enum View { OVERVIEW, TASK, USAGE, TOKENS } view_ = OVERVIEW;
    uint8_t task_index_ = 0, usage_window_ = 0;
    void update();
    static void timer_cb(lv_timer_t *t);
};
}
