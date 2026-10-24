#pragma once

#include "../Page.h"
#include "services/GestureFSM.h"

namespace Page {
class Menu : public PageBase {
public:
    void onCustomAttrConfig() override;
    void onViewLoad() override;
    void onViewWillAppear() override;
    void onViewWillDisappear() override;
    void onViewDidUnload() override;
private:
    static constexpr int kItemCount = 7;
    lv_obj_t *title_ = nullptr, *ring_ = nullptr, *inner_ring_ = nullptr;
    lv_obj_t *name_ = nullptr, *previous_name_ = nullptr, *index_ = nullptr;
    lv_obj_t *icon_parts_[3][5] = {};
    lv_obj_t *dots_[kItemCount] = {};
    lv_group_t *group_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    GestureFSM gesture_;
    int selected_ = 0;
    int previous_selected_ = 0, transition_direction_ = 1;
    uint32_t transition_at_ = 0, select_at_ = 0;
    bool select_pending_ = false;
    void update();
    void render(uint32_t now);
    void open_selected();
    static void timer_cb(lv_timer_t *timer);
};
}
