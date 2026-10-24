#include "Codex.h"
#include "../RoundUI.h"
#include "hal/hal.h"
#include "hal/motor.h"
#include <Arduino.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

using namespace Page;
namespace {
constexpr uint32_t kBg = 0x03080E, kText = 0xF1FBFF, kMuted = 0x8296A7;
lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                     int width, int height, int y) {
    lv_obj_t *o = lv_label_create(parent);
    lv_obj_set_size(o, width, height);
    lv_label_set_long_mode(o, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, lv_color_hex(color), 0);
    lv_obj_align(o, LV_ALIGN_CENTER, 0, y);
    return o;
}
void set_label(lv_obj_t *o, const char *s) { lv_label_set_text(o, s); }
const char *phase_label(const char *phase) {
    if (strcmp(phase, "running") == 0) return "工作中";
    if (strcmp(phase, "approval_pending") == 0) return "等待确认";
    if (strcmp(phase, "stale_unknown") == 0) return "状态未知";
    if (strcmp(phase, "completed") == 0) return "已结束";
    return "活动会话";
}
}

void Codex::onCustomAttrConfig() { SetCustomCacheEnable(false); }
void Codex::onViewLoad() {
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 240, 240);
    lv_obj_set_style_bg_color(root, lv_color_hex(kBg), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    title_ = make_label(root, Resource.GetFont("bahnschrift_17"), kMuted, 190, 20, -91);
    subtitle_ = make_label(root, &xknob_round_ui_12, 0x536678, 190, 20, -71);
    divider_ = lv_obj_create(root); lv_obj_remove_style_all(divider_);
    lv_obj_set_size(divider_, 106, 1); lv_obj_align(divider_, LV_ALIGN_CENTER, 0, 55);
    lv_obj_set_style_bg_color(divider_, lv_color_hex(0x213543), 0); lv_obj_set_style_bg_opa(divider_, LV_OPA_COVER, 0);
    value_ = make_label(root, Resource.GetFont("bahnschrift_65"), kText, 190, 80, -8);
    unit_ = make_label(root, &xknob_round_ui_20, 0x8DEBFF, 32, 30, -8);
    detail_ = make_label(root, &xknob_round_ui_20, kMuted, 190, 30, 34);
    footer_ = make_label(root, &xknob_round_ui_12, kMuted, 156, 28, 80);
    approval_dot_ = lv_obj_create(root); lv_obj_remove_style_all(approval_dot_);
    lv_obj_set_size(approval_dot_, 7, 7); lv_obj_set_style_radius(approval_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_align(approval_dot_, LV_ALIGN_CENTER, -72, 74);
    usage_arc_ = lv_arc_create(root); lv_obj_set_size(usage_arc_, 184, 184); lv_obj_align(usage_arc_, LV_ALIGN_CENTER, 0, -1);
    lv_arc_set_range(usage_arc_, 0, 100); lv_arc_set_bg_angles(usage_arc_, 148, 32); lv_arc_set_value(usage_arc_, 0);
    lv_obj_remove_style(usage_arc_, nullptr, LV_PART_KNOB); lv_obj_clear_flag(usage_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(usage_arc_, 7, LV_PART_MAIN); lv_obj_set_style_arc_color(usage_arc_, lv_color_hex(0x1A2C3A), LV_PART_MAIN);
    lv_obj_set_style_arc_width(usage_arc_, 7, LV_PART_INDICATOR); lv_obj_set_style_arc_color(usage_arc_, lv_color_hex(0x8DEBFF), LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(usage_arc_, true, LV_PART_INDICATOR);
    group_ = lv_group_create(); lv_group_add_obj(group_, root);
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_);
    lv_obj_add_flag(usage_arc_, LV_OBJ_FLAG_HIDDEN);
    update();
}
void Codex::onViewWillAppear() {
    HAL::mqtt_init();
    view_ = OVERVIEW;
    HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, HAL::get_motor_position());
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis());
    timer_ = lv_timer_create(timer_cb, 100, this);
}
void Codex::onViewDidDisappear() { if (timer_) { lv_timer_del(timer_); timer_ = nullptr; } }
void Codex::onViewDidUnload() { if (group_) { lv_group_del(group_); group_ = nullptr; } }
void Codex::timer_cb(lv_timer_t *t) { static_cast<Codex *>(t->user_data)->update(); }

void Codex::update() {
    // All DARK rotation belongs exclusively to the low-power direction switch.
    // Do not interpret it as menu/scene/task detents while LCD is off.
    if (HAL::power_is_dark()) return;
    if (HAL::power_home_requested()) return;
    HAL::CodexState s = {};
    const bool got = HAL::codex_state(&s) && s.snapshot_valid;
    const uint8_t session_count = got ? (s.session_count > 4 ? 4 : s.session_count) : 0;
    const uint8_t usage_window_count = got ? (s.usage_window_count > 2 ? 2 : s.usage_window_count) : 0;
    if (!session_count || task_index_ >= session_count) task_index_ = 0;
    if (!usage_window_count || usage_window_ >= usage_window_count) usage_window_ = 0;
    if (HAL::take_wake_press()) gesture_.consume_wake_press(HAL::encoder_is_pushed());
    const auto e = gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis());
    if (e.kind == GestureFSM::DOUBLE_CLICK) { Manager->BackHome(); return; }
    if (e.kind == GestureFSM::LONG_PRESS) { HAL::motor_exit_feedback(); Manager->Pop(); return; }
    if (e.kind == GestureFSM::ROTATE) {
        if (view_ == TASK && session_count) task_index_ = (task_index_ + e.delta % session_count + session_count) % session_count;
        else if (view_ == USAGE && usage_window_count) usage_window_ = (usage_window_ + e.delta % usage_window_count + usage_window_count) % usage_window_count;
        else if (view_ == OVERVIEW) view_ = e.delta > 0 ? TASK : USAGE;
        else if (view_ == TOKENS) view_ = OVERVIEW;
    } else if (e.kind == GestureFSM::SINGLE_CLICK) {
        if (view_ == OVERVIEW) view_ = TASK;
        else if (view_ == TASK) view_ = USAGE;
        else if (view_ == USAGE) view_ = TOKENS;
        else view_ = OVERVIEW;
    }
    const bool show_arc = view_ == USAGE;
    if (show_arc) lv_obj_clear_flag(usage_arc_, LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(usage_arc_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    char value[40], unit[4] = "", detail[64], footer[96];
    switch (view_) {
    case OVERVIEW:
        set_label(title_, "C O D E X"); set_label(subtitle_, "实时任务概览");
        snprintf(value, sizeof(value), "%02d", got ? s.running_count : 0); set_label(value_, value);
        set_label(detail_, got && s.running_count ? "任务进行中" : "暂无运行任务");
        lv_obj_set_style_bg_color(approval_dot_, lv_color_hex(got && s.approval_count ? 0xFFBD76 : 0x213543), 0);
        snprintf(footer, sizeof(footer), "%s  %02d", got && s.approval_count ? "等待审批" : "审批请求", got ? s.approval_count : 0);
        set_label(footer_, footer);
        break;
    case TASK:
        set_label(title_, "C O D E X");
        snprintf(value, sizeof(value), "任务 %02u / %02u", session_count ? task_index_ + 1 : 0, session_count); set_label(subtitle_, value);
        if (got && session_count) {
            snprintf(value, sizeof(value), "%02u:%02u", s.session_age_seconds[task_index_] / 60, s.session_age_seconds[task_index_] % 60);
            snprintf(detail, sizeof(detail), "%s", phase_label(s.session_phase[task_index_]));
            set_label(value_, value); set_label(detail_, detail);
        } else { set_label(value_, "--"); set_label(detail_, "没有活动会话"); }
        set_label(footer_, got ? "会话状态实时更新" : "会话数据不可用");
        break;
    case USAGE: {
        set_label(title_, "窗口剩余");
        lv_obj_add_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(divider_, LV_OBJ_FLAG_HIDDEN);
        const bool valid = got && s.usage_valid && usage_window_count;
        const int percent = valid ? constrain(s.window_remaining_percent[usage_window_], 0, 100) : 0;
        lv_arc_set_value(usage_arc_, percent);
        snprintf(value, sizeof(value), valid ? "%d" : "--", percent);
        if (valid) strcpy(unit, "%");
        set_label(value_, value);
        snprintf(detail, sizeof(detail), "%s", valid ? (usage_window_ == 0 ? "当前额度剩余" : "周期额度剩余") : "额度数据不可用"); set_label(detail_, detail);
        if (valid && s.window_duration_minutes[usage_window_] > 0) snprintf(footer, sizeof(footer), "周期 %d 分钟", s.window_duration_minutes[usage_window_]);
        else snprintf(footer, sizeof(footer), "额度不可用");
        set_label(footer_, footer); break;
    }
    case TOKENS: {
        set_label(title_, "C O D E X"); set_label(subtitle_, "T O K E N   U S A G E");
        const uint64_t n = got ? s.lifetime_tokens : 0;
        if (!got) snprintf(value, sizeof(value), "--");
        else if (n >= 1000000) { snprintf(value, sizeof(value), "%.2f", n / 1000000.0); strcpy(unit, "M"); }
        else if (n >= 1000) { snprintf(value, sizeof(value), "%.1f", n / 1000.0); strcpy(unit, "K"); }
        else snprintf(value, sizeof(value), "%llu", (unsigned long long)n);
        set_label(value_, value); set_label(detail_, got && s.daily_token_count ? "近 7 日 Token 活动" : "趋势数据不可用");
        if (got && s.daily_token_count) {
            snprintf(footer, sizeof(footer), "日均  %llu", (unsigned long long)(n / s.daily_token_count));
        } else snprintf(footer, sizeof(footer), "累计 Token 总量");
        set_label(footer_, footer); break;
    }
    }
    if (view_ == USAGE || view_ == TOKENS) {
        RoundUI::centered_pair(value_, unit_, value, unit,
            Resource.GetFont("bahnschrift_65"), &xknob_round_ui_20,
            kText, 0x8DEBFF, 104, 190);
    } else {
        lv_obj_add_flag(unit_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(value_, LV_ALIGN_CENTER, 0, -8);
        lv_obj_set_style_text_font(value_, Resource.GetFont("bahnschrift_65"), 0);
        lv_obj_set_style_text_color(value_, lv_color_hex(kText), 0);
    }
    // Keep detail text below the 65 px hero number and above the footer row.
    lv_obj_align(title_, LV_ALIGN_CENTER, 0, view_ == USAGE ? -63 : -91);
    lv_obj_set_style_text_font(title_, view_ == USAGE ? &xknob_round_ui_20 :
        Resource.GetFont("bahnschrift_17"), 0);
    if (view_ != USAGE) {
        lv_obj_clear_flag(subtitle_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(divider_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_align(detail_, LV_ALIGN_CENTER, 0, 37);
    lv_obj_align(divider_, LV_ALIGN_CENTER, 0, 63);
    lv_obj_align(footer_, LV_ALIGN_CENTER, 0, 82);
    if (view_ == USAGE) lv_obj_set_size(footer_, 136, 28);
    else lv_obj_set_size(footer_, 156, 28);
}
