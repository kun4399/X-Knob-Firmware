#include "V48Aux.h"
#include "../RoundUI.h"
#include "hal/hal.h"
#include "hal/motor.h"
#include "services/MenuNavigation.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>

using namespace Page;
namespace {
constexpr uint32_t kAccent = 0x8DEBFF;
const uint16_t kDimChoices[] = {15, 30, 45, 60};
const uint16_t kStandbyChoices[] = {120, 180, 300, 600};

lv_obj_t *aux_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int w, int h, int y, int x=0) {
    auto *o = lv_label_create(parent); lv_obj_set_size(o, w, h); lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0); lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0); lv_obj_align(o, LV_ALIGN_CENTER, x, y); return o;
}
int choice_index(uint16_t value, const uint16_t *choices, int count) {
    for (int i = 0; i < count; ++i) if (choices[i] == value) return i;
    return 0;
}
const char *notification_title(const char *kind) {
    if (!kind || !kind[0]) return "暂无新通知";
    if (strcmp(kind, "codex.task.started") == 0) return "任务已开始";
    if (strcmp(kind, "codex.task.completed_verified") == 0) return "任务已完成";
    if (strcmp(kind, "codex.task.failed") == 0) return "任务遇到问题";
    if (strcmp(kind, "codex.approval.requested") == 0) return "有操作等待确认";
    if (strcmp(kind, "codex.notification") == 0) return "收到新通知";
    return "Codex 状态已更新";
}
}
void V48Aux::onCustomAttrConfig() { SetCustomCacheEnable(false); }
void V48Aux::onViewLoad() {
    lv_obj_remove_style_all(root); lv_obj_set_size(root, 240, 240);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0); lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    title_ = aux_label(root, &xknob_round_ui_20, 0x91A6B5, 160, 26, -95);
    hero_ = aux_label(root, &xknob_round_ui_20, 0xF1FBFF, 200, 34, -2);
    detail_ = aux_label(root, &xknob_round_ui_12, 0x8296A7, 200, 24, 30);
    mark_ = lv_obj_create(root); lv_obj_remove_style_all(mark_); lv_obj_set_size(mark_, 6, 6);
    lv_obj_set_style_radius(mark_, LV_RADIUS_CIRCLE, 0); lv_obj_align(mark_, LV_ALIGN_CENTER, 0, -37);
    static const int row_y[] = {-44, -12, 20, 52};
    for (int i=0; i<4; ++i) {
        row_frames_[i] = lv_obj_create(root);
        lv_obj_remove_style_all(row_frames_[i]);
        lv_obj_set_size(row_frames_[i], 178, 29);
        lv_obj_align(row_frames_[i], LV_ALIGN_CENTER, 0, row_y[i]);
        lv_obj_set_style_radius(row_frames_[i], 8, 0);
        lv_obj_set_style_bg_opa(row_frames_[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(row_frames_[i], 1, 0);
        lv_obj_set_style_border_color(row_frames_[i], lv_color_hex(0x20313D), 0);
        rows_[i] = aux_label(root, &xknob_round_ui_20, 0x8197A6, 94, 26, row_y[i], -36);
        row_values_[i] = aux_label(root, &xknob_round_ui_20, 0x8197A6, 76, 26, row_y[i], 54);
        lv_obj_set_style_text_align(row_values_[i], LV_TEXT_ALIGN_RIGHT, 0);
    }
    for (int i = 0; i < 3; ++i) {
        dividers_[i] = lv_obj_create(root);
        lv_obj_remove_style_all(dividers_[i]);
        lv_obj_set_size(dividers_[i], 154, 1);
        lv_obj_align(dividers_[i], LV_ALIGN_CENTER, 0, row_y[i] + 16);
        lv_obj_set_style_bg_color(dividers_[i], lv_color_hex(0x20313D), 0);
        lv_obj_set_style_bg_opa(dividers_[i], LV_OPA_COVER, 0);
    }
    group_ = lv_group_create(); lv_group_add_obj(group_, root); lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_); render();
}
void V48Aux::onViewWillAppear() {
    HAL::mqtt_init(); HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, HAL::get_motor_position());
    editing_ = false;
    save_failed_ = false;
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis()); timer_ = lv_timer_create(timer_cb, 80, this);
}
void V48Aux::onViewDidDisappear() { if (timer_) { lv_timer_del(timer_); timer_=nullptr; } }
void V48Aux::onViewDidUnload() { if (group_) { lv_group_del(group_); group_=nullptr; } }
void V48Aux::timer_cb(lv_timer_t *t) { static_cast<V48Aux *>(t->user_data)->update(); }
void V48Aux::render() {
    HAL::CodexState s={}; const bool valid=HAL::codex_state(&s) && s.snapshot_valid;
    if(kind_==NOTIFICATIONS) {
        const bool has_event = valid && s.event_kind[0];
        lv_label_set_text(title_, "通知中心");
        lv_label_set_text(hero_, has_event ? notification_title(s.event_kind) : "暂无新通知");
        lv_label_set_text(detail_, has_event ? "查看 Codex 工作总览" : "收到新事件后会显示在这里");
        lv_obj_align(hero_, LV_ALIGN_CENTER, 0, -2);
        lv_obj_align(detail_, LV_ALIGN_CENTER, 0, 30);
        lv_obj_set_style_text_font(hero_, &xknob_round_ui_20, 0);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -89);
        lv_obj_clear_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        for (auto *o : rows_) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        for (auto *o : row_values_) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        for (auto *o : row_frames_) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        for (auto *o : dividers_) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(mark_, lv_color_hex(valid && s.event_kind[0] ? 0x7BECC4 : 0x31424D), 0);
    } else if (kind_ == POWER) {
        lv_label_set_text(title_, "P O W E R");
        lv_label_set_text(hero_, "自动省电");
        lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_font(title_, Resource.GetFont("bahnschrift_17"), 0);
        lv_obj_set_style_text_font(hero_, &xknob_round_ui_12, 0);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -95);
        lv_obj_align(hero_, LV_ALIGN_CENTER, 0, -71);
        HAL::PowerSettings settings = {};
        HAL::power_settings_get(&settings);
        if (!editing_) edit_settings_ = settings;
        const char *labels[]={"自动调暗","联网待机","自动关机","重要通知"};
        char values[4][16];
        snprintf(values[0], sizeof(values[0]), "%u 秒", (unsigned)edit_settings_.dim_after_seconds);
        if (edit_settings_.standby_after_seconds % 60 == 0)
            snprintf(values[1], sizeof(values[1]), "%u 分", (unsigned)(edit_settings_.standby_after_seconds / 60));
        else snprintf(values[1], sizeof(values[1]), "%u 秒", (unsigned)edit_settings_.standby_after_seconds);
        snprintf(values[2], sizeof(values[2]), "未校准");
        snprintf(values[3], sizeof(values[3]), "%s", edit_settings_.important_notifications ? "开启" : "关闭");
        for(int i=0;i<4;++i){
            lv_obj_clear_flag(row_frames_[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(rows_[i],LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(row_values_[i],LV_OBJ_FLAG_HIDDEN);
            const bool selected = i == selected_row_;
            const uint32_t text_color = selected ? 0xF1FBFF : 0x8197A6;
            const uint32_t value_color = selected ? kAccent : 0x8197A6;
            lv_obj_set_style_border_color(row_frames_[i], lv_color_hex(selected ?
                (editing_ ? kAccent : 0x355567) : 0x20313D), 0);
            lv_obj_set_style_bg_color(row_frames_[i], lv_color_hex(selected ? 0x0A131B : 0x03080E), 0);
            lv_obj_set_style_bg_opa(row_frames_[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(rows_[i], lv_color_hex(text_color), 0);
            lv_obj_set_style_text_color(row_values_[i], lv_color_hex(value_color), 0);
            lv_label_set_text(rows_[i], labels[i]);
            lv_label_set_text(row_values_[i], values[i]);
            if (save_failed_ && selected) lv_label_set_text(row_values_[i], "失败");
        }
        for (auto *line : dividers_) lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mark_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(title_, "SYSTEM");
        lv_label_set_text(hero_, "设备设置");
        lv_obj_set_style_text_font(title_, Resource.GetFont("bahnschrift_17"), 0);
        lv_obj_set_style_text_font(hero_, &xknob_round_ui_12, 0);
        lv_obj_align(title_, LV_ALIGN_CENTER, 0, -95);
        lv_obj_align(hero_, LV_ALIGN_CENTER, 0, -71);
        lv_obj_add_flag(detail_, LV_OBJ_FLAG_HIDDEN);
        HAL::SmartledState light = {};
        const bool online = HAL::smartled_state(&light) && light.online;
        const char *labels[] = {"电源与休眠", "WiFi / OTA", "屏幕亮度", "MQTT 状态"};
        const char *values[] = {"打开", "打开", "打开", online ? "在线" : "离线"};
        for (int i = 0; i < 4; ++i) {
            const bool selected = i == selected_row_;
            lv_obj_clear_flag(row_frames_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(rows_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(row_values_[i], LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_border_color(row_frames_[i], lv_color_hex(selected ? 0x8DEBFF : 0x20313D), 0);
            lv_obj_set_style_bg_color(row_frames_[i], lv_color_hex(selected ? 0x0A131B : 0x03080E), 0);
            lv_obj_set_style_bg_opa(row_frames_[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
            lv_obj_set_style_text_color(rows_[i], lv_color_hex(selected ? 0xF1FBFF : 0x8197A6), 0);
            lv_obj_set_style_text_color(row_values_[i], lv_color_hex(selected ? 0x8DEBFF : 0x8197A6), 0);
            lv_label_set_text(rows_[i], labels[i]);
            lv_label_set_text(row_values_[i], values[i]);
        }
        for (auto *line : dividers_) lv_obj_clear_flag(line, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(mark_, LV_OBJ_FLAG_HIDDEN);
    }
}
void V48Aux::update() {
    if(HAL::take_wake_press()) gesture_.consume_wake_press(HAL::encoder_is_pushed());
    const auto e=gesture_.update(HAL::encoder_is_pushed(),HAL::get_motor_position(),millis());
    if (e.kind == GestureFSM::DOUBLE_CLICK) { Manager->BackHome(); return; }
    if(e.kind==GestureFSM::LONG_PRESS){ HAL::motor_exit_feedback(); Manager->Pop(); return; }
    if(e.kind==GestureFSM::SINGLE_CLICK && kind_==NOTIFICATIONS){ Manager->Push("Pages/Codex"); return; }
    if (kind_ == SETTINGS) {
        if (e.kind == GestureFSM::ROTATE) {
            selected_row_ = menu_navigation_apply_delta(selected_row_, e.delta, 4);
            HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, HAL::get_motor_position());
            gesture_.rebase(HAL::get_motor_position());
            render();
        } else if (e.kind == GestureFSM::SINGLE_CLICK) {
            if (selected_row_ == 0) Manager->Push("Pages/Power");
            else if (selected_row_ == 1 || selected_row_ == 2) Manager->Push("Pages/Setting");
        }
        return;
    }
    if (kind_ == POWER) {
        bool changed = false;
        if (e.kind == GestureFSM::ROTATE) {
            save_failed_ = false;
            if (editing_) {
                if (selected_row_ == 0) {
                    edit_option_ = menu_navigation_apply_delta(edit_option_, e.delta, 4);
                    edit_settings_.dim_after_seconds = kDimChoices[edit_option_];
                } else if (selected_row_ == 1) {
                    edit_option_ = menu_navigation_apply_delta(edit_option_, e.delta, 4);
                    edit_settings_.standby_after_seconds = kStandbyChoices[edit_option_];
                } else if (selected_row_ == 3) {
                    if (e.delta % 2 != 0)
                        edit_settings_.important_notifications = !edit_settings_.important_notifications;
                }
            } else {
                selected_row_ = menu_navigation_apply_delta(selected_row_, e.delta, 4);
                HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, HAL::get_motor_position());
                gesture_.rebase(HAL::get_motor_position());
            }
            changed = true;
        } else if (e.kind == GestureFSM::SINGLE_CLICK) {
            save_failed_ = false;
            if (editing_) {
                if (!HAL::power_settings_set(&edit_settings_)) save_failed_ = true;
                editing_ = false;
                HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, HAL::get_motor_position());
                gesture_.rebase(HAL::get_motor_position());
            } else if (selected_row_ == 2) {
                // Battery/VBUS sensing is unknown on this board revision, so
                // automatic power-off stays unavailable until that is tested.
            } else {
                HAL::power_settings_get(&edit_settings_);
                if (selected_row_ == 0)
                    edit_option_ = choice_index(edit_settings_.dim_after_seconds, kDimChoices, 4);
                else if (selected_row_ == 1)
                    edit_option_ = choice_index(edit_settings_.standby_after_seconds, kStandbyChoices, 4);
                else edit_option_ = edit_settings_.important_notifications ? 1 : 0;
                editing_ = true;
                HAL::update_motor_mode(MOTOR_UNBOUND_FINE_DETENTS, HAL::get_motor_position());
                gesture_.rebase(HAL::get_motor_position());
            }
            changed = true;
        }
        if (changed) render();
    }
    HAL::CodexState snapshot = {};
    const bool valid = HAL::codex_state(&snapshot) && snapshot.snapshot_valid;
    const uint32_t revision = valid ? snapshot.event_revision : 0;
    if (rendered_revision_ != revision) { rendered_revision_ = revision; render(); }
}
