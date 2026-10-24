#include "Menu.h"
#include "../RoundUI.h"
#include "services/MenuNavigation.h"
#include "hal/hal.h"
#include "hal/motor.h"
#include <math.h>

using namespace Page;

namespace {
constexpr uint32_t kAccents[] = {0xFFD4A0, 0xFFBD76, 0x8DEBFF, 0x7BECC4, 0xC4B3FF, 0xC4B3FF};
constexpr const char *kNames[] = {"灯光控制", "场景编辑", "Codex 监控", "通知中心", "电源设置", "系统设置"};
constexpr uint32_t kSlideMs = 220;
constexpr uint32_t kSelectMs = 155;

lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                lv_coord_t width, lv_coord_t height, lv_coord_t y) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_size(obj, width, height);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, LV_ALIGN_CENTER, 0, y);
    return obj;
}

void shape(lv_obj_t *obj, int x, int y, int w, int h, uint32_t color,
           bool outline = false, int radius = 2, int border = 2) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_size(obj, w, h);
    lv_obj_align(obj, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_radius(obj, radius == 0 ? LV_RADIUS_CIRCLE : radius, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, outline ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, outline ? border : 0, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(color), 0);
}

void draw_symbol(lv_obj_t *(&parts)[5], int kind, int cx, int cy, float scale,
                 uint32_t color, bool visible) {
    for (auto *part : parts) lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
    if (!visible) return;
    auto put = [&](int i, int x, int y, int w, int h, bool outline=false,
                   int radius=2, int border=2) {
        shape(parts[i], cx + (int)(x*scale), cy + (int)(y*scale),
              (int)(w*scale), (int)(h*scale), color, outline, radius, border);
    };
    switch (kind) {
        case 0: // lamp
            put(0, 0, -4, 28, 30, true, 14, 2);
            put(1, 0, 11, 15, 3); put(2, 0, 17, 11, 3);
            put(3, 0, -22, 2, 7); put(4, 0, -4, 2, 11);
            break;
        case 1: // scene points
            put(0, -13, -4, 10, 10, true, 5, 2);
            put(1, 9, -12, 8, 8, true, 4, 2);
            put(2, 7, 9, 12, 12, true, 6, 2);
            put(3, 0, 19, 35, 2); put(4, 0, -4, 2, 2);
            break;
        case 2: // code chevrons
            put(0, -10, 0, 13, 3); put(1, -16, -6, 3, 15);
            put(2, 10, 0, 13, 3); put(3, 16, 6, 3, 15);
            put(4, 0, 0, 3, 36);
            break;
        case 3: // notification bell
            put(0, 0, -2, 25, 25, true, 13, 2);
            put(1, 0, 13, 17, 3); put(2, 0, 20, 6, 3);
            put(3, 0, -18, 4, 4); break;
        case 4: // power
            put(0, 0, 1, 34, 34, true, 0, 2);
            put(1, 0, -9, 3, 20); break;
        default: // settings
            put(0, 0, 0, 12, 12, true, 0, 2);
            put(1, 0, -18, 4, 9); put(2, 0, 18, 4, 9);
            put(3, -18, 0, 9, 4); put(4, 18, 0, 9, 4);
            break;
    }
}
}

void Menu::onCustomAttrConfig() { SetCustomCacheEnable(false); }

void Menu::onViewLoad() {
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 240, 240);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    title_ = label(root, &xknob_round_ui_20, 0x91A6B5, 150, 26, -92);
    lv_label_set_text(title_, "应用");

    ring_ = lv_obj_create(root);
    lv_obj_remove_style_all(ring_);
    lv_obj_set_size(ring_, 88, 88);
    lv_obj_align(ring_, LV_ALIGN_CENTER, 0, -14);
    lv_obj_set_style_radius(ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ring_, lv_color_hex(0x03080E), 0);
    lv_obj_set_style_bg_opa(ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ring_, 1, 0);
    lv_obj_set_style_border_color(ring_, lv_color_hex(0x263B49), 0);
    lv_obj_set_style_shadow_width(ring_, 10, 0);
    lv_obj_set_style_shadow_color(ring_, lv_color_hex(kAccents[0]), 0);
    lv_obj_set_style_shadow_opa(ring_, LV_OPA_20, 0);

    inner_ring_ = lv_obj_create(root);
    lv_obj_remove_style_all(inner_ring_);
    lv_obj_set_size(inner_ring_, 70, 70);
    lv_obj_align(inner_ring_, LV_ALIGN_CENTER, 0, -14);
    lv_obj_set_style_radius(inner_ring_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(inner_ring_, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(inner_ring_, 1, 0);
    lv_obj_set_style_border_color(inner_ring_, lv_color_hex(0x192B37), 0);

    for (auto &slot : icon_parts_) {
        for (auto &part : slot) {
            part = lv_obj_create(root);
            lv_obj_remove_style_all(part);
            lv_obj_add_flag(part, LV_OBJ_FLAG_HIDDEN);
        }
    }
    name_ = label(root, &xknob_round_ui_20, 0xF1FBFF, 190, 29, 51);
    previous_name_ = label(root, &xknob_round_ui_20, 0x8296A7, 190, 29, 51);
    lv_obj_add_flag(previous_name_, LV_OBJ_FLAG_HIDDEN);
    index_ = label(root, Resource.GetFont("bahnschrift_17"), 0x8296A7, 100, 20, 77);
    for (int i = 0; i < kItemCount; ++i) {
        dots_[i] = lv_obj_create(root);
        lv_obj_remove_style_all(dots_[i]);
        lv_obj_set_style_radius(dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dots_[i], LV_OPA_COVER, 0);
        lv_obj_align(dots_[i], LV_ALIGN_CENTER, -26 + i * 10, 100);
    }
    group_ = lv_group_create();
    lv_group_add_obj(group_, root);
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_);
    render(millis());
}

void Menu::onViewWillAppear() {
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_);
    // The carousel is silent on selection; only rotation changes its focus.
    // One lighter 30-degree detent selects exactly one application.
    HAL::update_motor_mode(MOTOR_UNBOUND_APP_MENU_DETENTS, HAL::get_motor_position());
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis());
    timer_ = lv_timer_create(timer_cb, 20, this);
}

void Menu::onViewWillDisappear() { if (timer_) { lv_timer_del(timer_); timer_ = nullptr; } }
void Menu::onViewDidUnload() { if (group_) { lv_group_del(group_); group_ = nullptr; } }
void Menu::timer_cb(lv_timer_t *timer) { static_cast<Menu *>(timer->user_data)->update(); }

void Menu::render(uint32_t now) {
    const uint32_t age = now - transition_at_;
    const float t = transition_at_ ? (age >= kSlideMs ? 1.0f : age / (float)kSlideMs) : 1.0f;
    const float ease = t * t * (3.0f - 2.0f * t);
    const int displacement = (int)((1.0f - ease) * 25.0f);
    const uint32_t accent = kAccents[selected_];

    lv_label_set_text(name_, kNames[selected_]);
    lv_obj_align(name_, LV_ALIGN_CENTER, -transition_direction_ * displacement, 51);
    lv_obj_set_style_opa(name_, (lv_opa_t)(110 + ease * 145), 0);
    if (t < 1.0f) {
        lv_label_set_text(previous_name_, kNames[previous_selected_]);
        lv_obj_align(previous_name_, LV_ALIGN_CENTER, transition_direction_ * (25 - displacement), 51);
        lv_obj_set_style_opa(previous_name_, (lv_opa_t)(160 * (1.0f - ease)), 0);
        lv_obj_clear_flag(previous_name_, LV_OBJ_FLAG_HIDDEN);
    } else lv_obj_add_flag(previous_name_, LV_OBJ_FLAG_HIDDEN);

    const uint32_t select_age = now - select_at_;
    const float pulse = select_pending_ && select_age < kSelectMs
        ? sinf((select_age / (float)kSelectMs) * 3.1415926f) : 0.0f;
    lv_obj_set_size(ring_, 88 + (int)(pulse * 6), 88 + (int)(pulse * 6));
    lv_obj_set_style_border_color(ring_, lv_color_hex(0x263B49), 0);
    lv_obj_set_style_shadow_color(ring_, lv_color_hex(accent), 0);
    lv_obj_set_style_shadow_width(ring_, 10 + (int)(pulse * 12), 0);
    lv_obj_set_style_shadow_opa(ring_, (lv_opa_t)(35 + pulse * 95), 0);
    lv_label_set_text_fmt(index_, "%02d / 06", selected_ + 1);

    const float slide = transition_at_ && age < kSlideMs ? 1.0f - ease : 0.0f;
    const int incoming_x = (int)(transition_direction_ * 67 * slide);
    draw_symbol(icon_parts_[1], selected_, incoming_x, -14, 1.0f, accent, true);
    draw_symbol(icon_parts_[0], (selected_ + kItemCount - 1) % kItemCount,
                -67, -11, .68f, 0x557382, !(slide > .01f && transition_direction_ < 0));
    draw_symbol(icon_parts_[2], (selected_ + 1) % kItemCount,
                67, -11, .68f, 0x557382, !(slide > .01f && transition_direction_ > 0));

    for (int i = 0; i < kItemCount; ++i) {
        const bool active = i == selected_;
        lv_obj_set_style_bg_color(dots_[i], lv_color_hex(active ? accent : 0x2A3D4D), 0);
        lv_obj_set_size(dots_[i], active ? 6 : 4, 4);
    }
}

void Menu::open_selected() {
    switch (selected_) {
        case 0: Manager->Push("Pages/Smartled"); break;
        case 1: Manager->Push("Pages/SceneEdit"); break;
        case 2: Manager->Push("Pages/Codex"); break;
        case 3: Manager->Push("Pages/Notifications"); break;
        case 4: Manager->Push("Pages/Power"); break;
        case 5: Manager->Push("Pages/Settings"); break;
    }
}

void Menu::update() {
    const uint32_t now = millis();
    if (HAL::take_wake_press()) gesture_.consume_wake_press(HAL::encoder_is_pushed());
    const auto e = gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), now);
    if (e.kind == GestureFSM::DOUBLE_CLICK) {
        Manager->Pop();
        return;
    }
    if (e.kind == GestureFSM::LONG_PRESS) {
        HAL::motor_exit_feedback();
        Manager->Pop();
        return;
    }
    if (!select_pending_ && e.kind == GestureFSM::ROTATE) {
        previous_selected_ = selected_;
        transition_direction_ = e.delta > 0 ? 1 : -1;
        selected_ = menu_navigation_apply_delta(selected_, e.delta, kItemCount);
        transition_at_ = now;
    } else if (!select_pending_ && e.kind == GestureFSM::SINGLE_CLICK) {
        select_pending_ = true;
        select_at_ = now;
    }
    const bool finish_transition = transition_at_ && now - transition_at_ >= kSlideMs;
    if (e.kind == GestureFSM::ROTATE || e.kind == GestureFSM::SINGLE_CLICK || transition_at_ || select_pending_) {
        render(now);
        if (finish_transition) transition_at_ = 0;
    }
    if (select_pending_ && now - select_at_ >= kSelectMs) {
        select_pending_ = false;
        open_selected();
    }
}
