#include "Face.h"
#include "../RoundUI.h"
#include "services/LightControl.h"
#include "face_data.h"
#include "hal/motor.h"
#include "RGB.h"
#include <Arduino.h>
#include <esp_sleep.h>
#include <math.h>
#include <stdio.h>

using namespace Page;
namespace {
int clamp_int(int value, int low, int high) { return value < low ? low : value > high ? high : value; }
float smoother(float t) { t = fmaxf(0, fminf(1, t)); return t*t*t*(t*(t*6-15)+10); }
float pulse(float t, float start, float duration) {
    if (t < start || t > start + duration) return 0;
    float u = (t - start) / duration;
    return u < .32f ? smoother(u / .32f) : 1 - smoother((u - .32f) / .68f);
}
bool equal(const char *a, const char *b) { return strcmp(a, b) == 0; }
}

void Face::onCustomAttrConfig() { SetCustomCacheEnable(false); }

void Face::onViewLoad() {
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 240, 240);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    drawing_ = lv_obj_create(root);
    lv_obj_remove_style_all(drawing_);
    lv_obj_set_size(drawing_, 240, 240);
    lv_obj_align(drawing_, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_event_cb(drawing_, draw_cb, LV_EVENT_DRAW_MAIN_END, this);
    caption_ = RoundUI::label(root, &xknob_round_ui_20, 0xCDE2EB,
                              RoundUI::kLowerTextWidth, 199);
    number_ = RoundUI::label(root, Resource.GetFont("bahnschrift_65"), 0xA4ECFF,
                             200, 84);
    number_unit_ = RoundUI::label(root, &xknob_round_ui_20, 0xFFD4A0, 40, 84);
    hint_ = RoundUI::label(root, &xknob_round_ui_12, 0x8296A7,
                           RoundUI::kLowerTextWidth, 192);
    lv_obj_set_height(hint_, 34);
    lv_label_set_long_mode(hint_, LV_LABEL_LONG_WRAP);
    overlay_title_ = RoundUI::label(root, Resource.GetFont("bahnschrift_17"), 0x8296A7,
                                    190, 55);
    lv_obj_set_style_text_letter_space(overlay_title_, 1, 0);

    off_icon_arc_ = lv_arc_create(root);
    lv_obj_set_size(off_icon_arc_, 88, 88);
    lv_obj_align(off_icon_arc_, LV_ALIGN_CENTER, 0, -2);
    lv_arc_set_bg_angles(off_icon_arc_, 50, 310);
    lv_arc_set_value(off_icon_arc_, 0);
    lv_obj_remove_style(off_icon_arc_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(off_icon_arc_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(off_icon_arc_, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_color(off_icon_arc_, lv_color_hex(0x91A6B4), LV_PART_MAIN);
    lv_obj_set_style_arc_opa(off_icon_arc_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_arc_width(off_icon_arc_, 0, LV_PART_INDICATOR);
    lv_obj_add_flag(off_icon_arc_, LV_OBJ_FLAG_HIDDEN);

    off_icon_stem_ = lv_obj_create(root);
    lv_obj_remove_style_all(off_icon_stem_);
    lv_obj_set_size(off_icon_stem_, 5, 42);
    lv_obj_align(off_icon_stem_, LV_ALIGN_CENTER, 0, -28);
    lv_obj_set_style_radius(off_icon_stem_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(off_icon_stem_, lv_color_hex(0x91A6B4), 0);
    lv_obj_set_style_bg_opa(off_icon_stem_, LV_OPA_COVER, 0);
    lv_obj_add_flag(off_icon_stem_, LV_OBJ_FLAG_HIDDEN);

    gauge_ring_ = lv_arc_create(root);
    lv_obj_set_size(gauge_ring_, 234, 234);
    lv_obj_align(gauge_ring_, LV_ALIGN_CENTER, 0, -1);
    lv_arc_set_range(gauge_ring_, 0, 100);
    lv_arc_set_bg_angles(gauge_ring_, 148, 32);
    lv_obj_remove_style(gauge_ring_, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(gauge_ring_, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(gauge_ring_, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_color(gauge_ring_, lv_color_hex(0x203241), LV_PART_MAIN);
    lv_obj_set_style_arc_width(gauge_ring_, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(gauge_ring_, true, LV_PART_INDICATOR);

    static const uint32_t cct_colors[] = {
        0xFFA971, 0xFFBC8C, 0xFFD0AD, 0xFFE9D7, 0xD8EAF4, 0xB3DDFC, 0x88CBFF
    };
    for (int i = 0; i < 7; ++i) {
        temperature_segments_[i] = lv_arc_create(root);
        lv_obj_set_size(temperature_segments_[i], 234, 234);
        lv_obj_align(temperature_segments_[i], LV_ALIGN_CENTER, 0, -1);
        lv_arc_set_range(temperature_segments_[i], 0, 100);
        const int start = (148 + i * 35) % 360;
        lv_arc_set_bg_angles(temperature_segments_[i], start, (start + 33) % 360);
        lv_arc_set_value(temperature_segments_[i], 0);
        lv_obj_remove_style(temperature_segments_[i], nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(temperature_segments_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(temperature_segments_[i], 7, LV_PART_MAIN);
        lv_obj_set_style_arc_color(temperature_segments_[i], lv_color_hex(cct_colors[i]), LV_PART_MAIN);
        lv_obj_set_style_arc_width(temperature_segments_[i], 0, LV_PART_INDICATOR);
    }
    temperature_knob_ = lv_obj_create(root);
    lv_obj_remove_style_all(temperature_knob_);
    lv_obj_set_size(temperature_knob_, 11, 11);
    lv_obj_set_style_radius(temperature_knob_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(temperature_knob_, lv_color_hex(0xF6FAFF), 0);
    lv_obj_set_style_bg_opa(temperature_knob_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(temperature_knob_, 1, 0);
    lv_obj_set_style_border_color(temperature_knob_, lv_color_hex(0x080D13), 0);

    scene_title_ = RoundUI::label(root, Resource.GetFont("bahnschrift_17"), 0x8C9DAC, 150, 22);
    lv_obj_set_style_text_letter_space(scene_title_, 2, 0);
    scene_index_ = RoundUI::label(root, Resource.GetFont("bahnschrift_17"), 0x718492, 100, 43);
    scene_name_ = RoundUI::label(root, &xknob_round_ui_28, 0xF1FBFF, 150, 94);
    scene_stats_ = RoundUI::label(root, &xknob_round_ui_12, 0x8296A7, 180, 141);
    for (int i = 0; i < SceneStore::count; ++i) {
        scene_dots_[i] = lv_obj_create(root);
        lv_obj_remove_style_all(scene_dots_[i]);
        lv_obj_set_style_radius(scene_dots_[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(scene_dots_[i], LV_OPA_COVER, 0);
    }
    for (int i = 0; i < 2; ++i) {
        scene_arcs_[i] = lv_arc_create(root);
        const int diameter = i == 0 ? 108 : 124;
        lv_obj_set_size(scene_arcs_[i], diameter, diameter);
        lv_obj_align(scene_arcs_[i], LV_ALIGN_CENTER, 0, -1);
        lv_arc_set_range(scene_arcs_[i], 0, 100);
        lv_arc_set_bg_angles(scene_arcs_[i], 310, 50);
        lv_obj_remove_style(scene_arcs_[i], nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(scene_arcs_[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(scene_arcs_[i], i == 0 ? 2 : 1, LV_PART_MAIN);
        lv_obj_set_style_arc_color(scene_arcs_[i], lv_color_hex(i == 0 ? 0x263848 : 0xFFBD76), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(scene_arcs_[i], i == 0 ? LV_OPA_COVER : LV_OPA_50, LV_PART_MAIN);
        lv_obj_set_style_arc_width(scene_arcs_[i], 0, LV_PART_INDICATOR);
    }
    group_ = lv_group_create();
    lv_group_add_obj(group_, root);
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_);
    scenes_.load();
    selected_scene_ = SceneStore::selected();
    render_labels();
}

void Face::onViewWillAppear() {
    cancel_command();
    HAL::mqtt_init();
    HAL::smartled_command("state"); // Legacy request during compatibility migration.
    screen_ = NORMAL;
    scenes_.load();
    selected_scene_ = SceneStore::selected();
    uint32_t now = millis();
    ignore_motor_until_ = now + 150;
    gesture_.rebase(HAL::get_motor_position());
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), now);
    if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0)
        gesture_.consume_wake_press(HAL::encoder_is_pushed());
    face_at_ = now;
    overlay_at_ = now;
    control_motor_mode_ = -1; // A child page may have changed the motor mode.
    sync_control_motor_mode(now);
    timer_ = lv_timer_create(timer_cb, 33, this);
}

void Face::onViewDidDisappear() {
    cancel_command();
    if (timer_) { lv_timer_del(timer_); timer_ = nullptr; }
}

void Face::onViewDidUnload() {
    if (group_) { lv_group_del(group_); group_ = nullptr; }
}

void Face::sync_control_motor_mode(uint32_t now) {
    int mode;
    int position = 0;
    if (screen_ == SCENES) {
        mode = MOTOR_UNBOUND_COARSE_DETENTS;
        position = HAL::get_motor_position();
    } else if (screen_ == TEMPERATURE) {
        mode = MOTOR_BOUNDED_CCT_DETENTS;
        position = light_control_face_cct_position(temperature_);
    } else if (screen_ == BRIGHTNESS) {
        mode = MOTOR_BOUNDED_BRIGHTNESS_DETENTS;
        position = light_control_brightness_detent_position(brightness_);
    } else {
        // User-selected switch feel: two endpoints with one snap between
        // OFF (0) and ON (1), using the existing bounded switch profile.
        // A pending opposite intent wins over an older gateway readback.
        mode = MOTOR_ON_OFF_STRONG_DETENTS;
        position = (pending_ || dirty_) ? (power_ ? 1 : 0) :
            light_.online ? (light_.power ? 1 : 0) :
            (HAL::get_motor_position() > 0 ? 1 : 0);
    }
    if (control_motor_mode_ == mode &&
        (mode != MOTOR_ON_OFF_STRONG_DETENTS ||
         HAL::get_motor_position() == position)) return;
    HAL::update_motor_mode(mode, position);
    control_motor_mode_ = static_cast<int8_t>(mode);
    if (mode == MOTOR_BOUNDED_CCT_DETENTS)
        cct_detent_position_ = position;
    gesture_.rebase(HAL::get_motor_position());
    ignore_motor_until_ = now + 150;
}

void Face::timer_cb(lv_timer_t *t) { static_cast<Face *>(t->user_data)->update(); }
void Face::draw_cb(lv_event_t *e) {
    auto *self = static_cast<Face *>(lv_event_get_user_data(e));
    self->draw(static_cast<const lv_area_t *>(lv_event_get_param(e)));
}

void Face::handle(GestureFSM::Event event, uint32_t now) {
    if (event.kind == GestureFSM::NONE) return;
    if (event.kind == GestureFSM::DOUBLE_CLICK) {
        if (screen_ != NORMAL) {
            screen_ = NORMAL;
            return;
        }
        Manager->Push("Pages/Menu");
        return;
    }
    if (event.kind == GestureFSM::LONG_PRESS) {
        if (screen_ == SCENES) {
            screen_ = NORMAL;
            HAL::motor_exit_feedback();
        } else {
            screen_ = SCENES;
        }
        overlay_at_ = now;
        return;
    }
    if (event.kind == GestureFSM::SINGLE_CLICK) {
        if (screen_ == SCENES) {
            if (!light_.online) { failed_ = true; return; }
            const LightScene scene = scenes_.get(selected_scene_);
            brightness_ = scene.brightness;
            temperature_ = scene.temperature;
            power_ = true;
            change_power_ = change_brightness_ = change_temperature_ = true;
            dirty_ = true;
            changed_at_ = now - 150;
            screen_ = BRIGHTNESS;
        } else if (screen_ == BRIGHTNESS) {
            screen_ = TEMPERATURE;
        } else if (screen_ == TEMPERATURE) {
            screen_ = BRIGHTNESS;
        } else if (!light_.online) {
            failed_ = true;
            screen_ = NORMAL;
        } else if (!light_.power) {
            // Precision adjustment needs a lit lamp. Explain the new easy
            // direction gesture instead of silently entering a locked dial.
            screen_ = LIGHT_OFF;
        } else {
            // Hard physical click is NOT a daily on/off switch.
            screen_ = BRIGHTNESS;
        }
        overlay_at_ = now;
        return;
    }
    if (event.kind == GestureFSM::HOLD_ROTATE) {
        // Hard press can drag the encoder. Face never maps HOLD_ROTATE to
        // a light command; precision CCT is selected using a short click.
        return;
    }
    if (event.kind == GestureFSM::ROTATE) {
        if (screen_ == NORMAL || screen_ == LIGHT_OFF) {
            // This is absolute direction, NOT toggle. Reverse movement
            // remains meaningful even if a prior BLE command is still pending.
            const bool desired = event.delta > 0;
            if (!light_.online) { failed_ = true; overlay_at_ = now; return; }
            if ((!last_power_intent_known_ || last_power_intent_ != desired || failed_) &&
                (pending_ || dirty_ || light_.power != desired)) {
                failed_ = false; // user explicitly retried a failed command
                power_ = desired;
                last_power_intent_ = desired;
                last_power_intent_known_ = true;
                change_power_ = true;
                change_brightness_ = change_temperature_ = false;
                dirty_ = true;
                changed_at_ = now - 150; // dispatch direction commands promptly
                overlay_at_ = now;
            }
            return;
        }
        if (screen_ == SCENES) {
            selected_scene_ = (selected_scene_ + event.delta % SceneStore::count + SceneStore::count) % SceneStore::count;
            SceneStore::select(selected_scene_);
            overlay_at_ = now;
            return;
        }
        if (!light_control_can_adjust(light_.online, light_.power && power_)) {
            failed_ = !light_.online;
            return;
        }
        if (screen_ == TEMPERATURE) {
            cct_detent_position_ = light_control_apply_delta(
                cct_detent_position_, event.delta, 0, kFaceCctDetentCount);
            temperature_ = light_control_face_cct_temperature(cct_detent_position_);
            change_temperature_ = power_;
        } else {
            brightness_ = clamp_int(brightness_ + event.delta, 1, 100);
            screen_ = BRIGHTNESS;
            change_brightness_ = power_;
        }
        dirty_ = power_;
        changed_at_ = overlay_at_ = now;
    }
}

void Face::send(uint32_t now) {
    if (!dirty_) return;
    const bool target_power = change_power_ ? power_ : light_.power;
    const bool requests_adjustment = change_brightness_ || change_temperature_;
    if (!light_.online || (requests_adjustment && !target_power) ||
        !HAL::smartled_set(change_power_, power_, change_brightness_,
            brightness_, change_temperature_, temperature_, pending_id_, sizeof(pending_id_))) {
        failed_ = true;
        dirty_ = false;
        return;
    }
    pending_ = true;
    dirty_ = false;
    failed_ = false;
    sent_at_ = now;
}

void Face::cancel_command() {
    dirty_ = pending_ = false;
    change_power_ = change_brightness_ = change_temperature_ = false;
    last_power_intent_known_ = false;
    pending_id_[0] = 0;
    retries_ = 0;
}

void Face::choose_face(uint32_t now) {
    if (screen_ != NORMAL) return;
    uint8_t chosen = 0;
    if (codex_.source_online && codex_.snapshot_valid &&
        equal(codex_.phase, "approval_pending")) chosen = 26;
    else if (codex_.source_online && codex_.snapshot_valid &&
             codex_event_at_ && now - codex_event_at_ < 3300 &&
             equal(codex_.phase, "running")) chosen = 25;
    else if (codex_event_at_ && now - codex_event_at_ < 3300 &&
             equal(codex_.event_kind, "codex.task.failed")) chosen = 28;
    else if (codex_event_at_ && now - codex_event_at_ < 3300 &&
             equal(codex_.event_kind, "codex.task.completed_verified")) chosen = 27;
    else if (codex_event_at_ && now - codex_event_at_ < 3300 &&
             equal(codex_.event_kind, "codex.notification")) chosen = 31;
    else if (now - face_at_ > 12000) { chosen = esp_random() % 25; face_at_ = now; }
    else chosen = face_id_ < 25 ? face_id_ : 0;
    if (chosen != face_id_) { face_id_ = chosen; face_at_ = now; }
}

void Face::render_labels() {
    const auto hide = [](lv_obj_t *obj) { if (obj) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN); };
    const auto show = [](lv_obj_t *obj) { if (obj) lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN); };
    const bool scenes = screen_ == SCENES;
    const bool face = screen_ == NORMAL || !light_.online;
    const bool light_off = screen_ == LIGHT_OFF;
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0);
    hide(number_); hide(number_unit_); hide(hint_); hide(overlay_title_);
    hide(scene_title_); hide(scene_index_); hide(scene_name_); hide(scene_stats_);
    hide(gauge_ring_); hide(temperature_knob_);
    hide(off_icon_arc_); hide(off_icon_stem_);
    for (auto *obj : temperature_segments_) hide(obj);
    for (auto *obj : scene_arcs_) hide(obj);
    for (auto *obj : scene_dots_) hide(obj);

    if (scenes) {
        show(scene_title_); show(scene_index_); show(scene_name_); show(scene_stats_);
        for (auto *obj : scene_arcs_) show(obj);
        for (int i = 0; i < SceneStore::count; ++i) {
            const float angle = (-45.0f + 30.0f * i) * 0.0174532925f;
            const int x = 120 + (int)lroundf(cosf(angle) * 76.0f);
            const int y = 119 + (int)lroundf(sinf(angle) * 76.0f);
            const bool selected = i == selected_scene_;
            const int size = selected ? 8 : 6;
            lv_obj_set_size(scene_dots_[i], size, size);
            lv_obj_align(scene_dots_[i], LV_ALIGN_TOP_LEFT, x - size / 2, y - size / 2);
            lv_obj_set_style_bg_color(scene_dots_[i], lv_color_hex(selected ? 0xFFBD76 : 0x324555), 0);
            show(scene_dots_[i]);
        }
        lv_label_set_text(scene_title_, "S C E N E S");
        lv_label_set_text_fmt(scene_index_, "%02d / 04", selected_scene_ + 1);
        lv_label_set_text(scene_name_, SceneStore::name(selected_scene_));
        const LightScene scene = scenes_.get(selected_scene_);
        lv_label_set_text_fmt(scene_stats_, "%d%%   ·   %dK", scene.brightness, scene.temperature);
        const float breath = .5f + .5f * sinf(millis() * .0031f);
        lv_obj_set_style_arc_opa(scene_arcs_[1], (lv_opa_t)(90 + breath * 50), LV_PART_MAIN);
        return;
    }

    if (light_off) {
        hide(drawing_);
        show(overlay_title_); show(off_icon_arc_); show(off_icon_stem_);
        show(caption_); show(hint_);
        lv_obj_set_style_text_font(caption_, &xknob_round_ui_20, 0);
        lv_obj_set_style_text_opa(caption_, LV_OPA_COVER, 0);
        lv_obj_align(overlay_title_, LV_ALIGN_TOP_MID, 0, 42);
        lv_label_set_text(overlay_title_, "LIGHT · OFF");
        lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, 171);
        lv_label_set_text(caption_, "已关闭");
        lv_obj_align(hint_, LV_ALIGN_TOP_MID, 0, 200);
        lv_obj_set_height(hint_, 30);
        lv_label_set_text(hint_, "顺时针旋转开灯");
        lv_obj_set_style_text_color(hint_, lv_color_hex(0x8296A7), 0);
        return;
    }

    if (face) {
        lv_obj_set_style_text_font(caption_, &xknob_round_ui_20, 0);
        show(drawing_); show(caption_);
        lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, 199);
        const char *text = "";
        const bool codex_live = codex_.source_online && codex_.snapshot_valid;
        // Mac sleeping must not leave a stale work/approval caption on the pet.
        if (!light_.online && failed_ && millis() - overlay_at_ < 3500)
            text = "灯带暂不可用";
        else if (codex_live && equal(codex_.phase, "approval_pending")) text = "待审批";
        else if (codex_live && codex_event_at_ && millis() - codex_event_at_ < 3300 &&
                 equal(codex_.phase, "running")) text = "工作中";
        else if (codex_live && equal(codex_.phase, "turn_stopped_unverified")) text = "本轮结束";
        else if (codex_live && codex_event_at_ && millis() - codex_event_at_ < 3300 &&
                 equal(codex_.event_kind, "codex.task.completed_verified")) text = "任务完成";
        else if (codex_live && codex_event_at_ && millis() - codex_event_at_ < 3300 &&
                 equal(codex_.event_kind, "codex.task.failed")) text = "任务失败";
        else if (codex_live && codex_event_at_ && millis() - codex_event_at_ < 3300 &&
                 equal(codex_.event_kind, "codex.notification")) text = "新消息";
        lv_label_set_text(caption_, text);
        const uint32_t age = millis() - face_at_;
        const float linger = age < 1250 ? 1.0f :
            1.0f - .40f * smoother(fminf(1.0f, (age - 1250) / 1800.0f));
        lv_obj_set_style_text_opa(caption_, (lv_opa_t)(255 * linger), 0);
        return;
    }

    hide(drawing_); hide(caption_);
    lv_obj_set_style_text_font(caption_, &xknob_round_ui_12, 0);
    show(gauge_ring_); show(overlay_title_); show(number_);
    show(caption_); show(hint_);
    // Split the category and confirmation rows; the normal face caption sits
    // lower on the circular panel and would otherwise collide with the hint.
    lv_obj_align(caption_, LV_ALIGN_TOP_MID, 0, 164);
    lv_obj_align(hint_, LV_ALIGN_TOP_MID, 0, 194);
    const bool brightness_mode = screen_ == BRIGHTNESS;
    const uint32_t accent = brightness_mode ? 0xFFD4A0 : 0x8DEBFF;
    lv_label_set_text(overlay_title_, brightness_mode ? "BRIGHTNESS" : "COLOR TEMP");
    char value_text[12];
    snprintf(value_text, sizeof(value_text), "%d", brightness_mode ? brightness_ : temperature_);
    RoundUI::centered_pair(number_, number_unit_, value_text, brightness_mode ? "%" : "K",
        Resource.GetFont("bahnschrift_65"), &xknob_round_ui_20, 0xF1FBFF, accent, 112);
    lv_label_set_text(caption_, brightness_mode ? "亮 度" :
        temperature_ < 3500 ? "暖 黄 白" : temperature_ > 5000 ? "冷 白" : "中 性 白");
    const char *feedback = !light_.online ? "连接断开" : failed_ ? "未确认" :
        pending_ || dirty_ ? "正在确认" : "已确认";
    lv_label_set_text(hint_, feedback);
    lv_obj_set_style_text_color(hint_, !light_.online || failed_ ? lv_color_hex(0xFF8DA0) :
        pending_ || dirty_ ? lv_color_hex(0xFFD4A0) : lv_color_hex(0x8296A7), 0);
    if (brightness_mode) {
        lv_arc_set_value(gauge_ring_, brightness_);
        lv_obj_set_style_arc_color(gauge_ring_, lv_color_hex(accent), LV_PART_INDICATOR);
    } else {
        hide(gauge_ring_);
        for (auto *obj : temperature_segments_) show(obj);
        const float fraction = (temperature_ - 2700) / 3800.0f;
        const float angle = (148.0f + kFaceCctSweepDegrees * fraction) * 0.0174532925f;
        lv_obj_align(temperature_knob_, LV_ALIGN_CENTER,
                     (lv_coord_t)lroundf(cosf(angle) * 117.0f),
                     (lv_coord_t)lroundf(sinf(angle) * 117.0f) - 1);
        show(temperature_knob_);
    }
}

void Face::update() {
    uint32_t now = millis();
    // Dark turns are handled globally by the always-ready power task. No
    // LVGL animation and no page gesture can consume a black-screen turn.
    if (HAL::power_is_dark()) {
        cancel_command();
        screen_ = NORMAL;
        return;
    }
    if (HAL::power_home_requested()) return;
    const auto input = HAL::motor_input();
    if (input.epoch != motor_epoch_) {
        motor_epoch_ = input.epoch;
        gesture_.rebase(input.position);
        ignore_motor_until_ = now + 80;
    }
    if (HAL::take_wake_press()) {
        cancel_command();
        screen_ = NORMAL;
        gesture_ = GestureFSM();
        gesture_.update(HAL::encoder_is_pushed(), input.position, now);
        gesture_.consume_wake_press(HAL::encoder_is_pushed());
        ignore_motor_until_ = now + 180;
    }
    HAL::SmartledState light;
    if (HAL::smartled_state(&light)) {
        light_ = light;
        if (!light.online && (pending_ || dirty_)) {
            cancel_command();
            failed_ = true;
        }
        if (screen_ == LIGHT_OFF && light_.online && light_.power)
            screen_ = NORMAL;
        if ((screen_ == BRIGHTNESS || screen_ == TEMPERATURE) &&
            light_.online && !light_.power)
            screen_ = NORMAL;
        if (light.received_ms && light.received_ms != light_seen_at_) {
            light_seen_at_ = light.received_ms;
            if (pending_ && light_control_readback_for_command(
                    light.modern, light.online, light.ok, light.last_command_id,
                    pending_id_, light.received_ms, sent_at_)) {
                bool matched = (!change_power_ || light.power == power_) &&
                    (!change_brightness_ || light.brightness == brightness_) &&
                    (!change_temperature_ || light.temperature == temperature_);
                if (matched && light.ok) {
                    pending_ = failed_ = false;
                    change_power_ = change_brightness_ = change_temperature_ = false;
                    retries_ = 0;
                }
            } else if (!pending_ && !dirty_ && light.online) {
                bool control_value_changed = brightness_ != light.brightness ||
                                             temperature_ != light.temperature;
                brightness_ = light.brightness;
                temperature_ = light.temperature;
                power_ = light.power;
                if (control_value_changed) control_motor_mode_ = -1;
                last_power_intent_known_ = false;
            }
        }
        if (pending_ && equal(light.last_ack_id, pending_id_) && !light.last_ack_success &&
            light.ack_received_ms >= sent_at_) {
            pending_ = false; failed_ = true;
        }
    }
    // Never change the face into a blocked hard-stop page merely because the
    // bulb is off. Off-state rotation is the PRIMARY physical operation.
    if (screen_ != SCENES && !light_.online && screen_ != NORMAL)
        screen_ = NORMAL;
    HAL::codex_state(&codex_);
    if (codex_.event_revision != event_revision_) {
        event_revision_ = codex_.event_revision;
        codex_event_at_ = now;
        // Do not awaken the LCD merely because Codex used a tool or published
        // an event. Important events stay in the snapshot for next wake.
    }
    if (static_cast<int32_t>(now - ignore_motor_until_) < 0 || input.sensor_only)
        gesture_.rebase(input.position);
    else handle(gesture_.update(HAL::encoder_is_pushed(), input.position, now), now);
    if (dirty_ && now - changed_at_ >= 150) send(now);
    if (pending_ && now - sent_at_ > 7000) {
        // A later user action may supersede this intent while a readback is
        // delayed. Timeout never replays an old command with a fresh ID.
        cancel_command();
        failed_ = true;
    }
    if ((screen_ == BRIGHTNESS || screen_ == TEMPERATURE) && now - overlay_at_ > 1800) screen_ = NORMAL;
    if (light_control_off_ui_expired(screen_ == LIGHT_OFF, now, overlay_at_))
        screen_ = NORMAL;
    sync_control_motor_mode(now);
    choose_face(now);
    StripMode led = screen_ == SCENES ? STRIP_SCENES :
        screen_ == BRIGHTNESS ? STRIP_LIGHT_BRIGHTNESS :
        screen_ == TEMPERATURE ? STRIP_LIGHT_CCT :
        face_id_ == 26 ? STRIP_APPROVAL : face_id_ == 25 ? STRIP_WORKING :
        face_id_ == 30 ? STRIP_OFFLINE : STRIP_IDLE;
    const bool important_event = equal(codex_.event_kind, "codex.approval.requested");
    strip_set_state(led, brightness_, temperature_, important_event ? event_revision_ : 0);
    render_labels();
    // The SVG-to-LVGL face renderer expands filled paths into software-drawn
    // triangles. Keep gesture/state sampling at 30 Hz, but cap face redraws at
    // 10 Hz so LVGL yields between animation frames instead of monopolizing
    // the core long enough to starve the idle task watchdog.
    if (screen_ == NORMAL &&
        (last_face_render_at_ == 0 || now - last_face_render_at_ >=
            (HAL::power_is_dim() ? 250U : 100U))) {
        last_face_render_at_ = now;
        lv_obj_invalidate(drawing_);
    }
}

void Face::draw(const lv_area_t *clip) {
    if (HAL::power_is_dark()) return;
    if (screen_ != NORMAL || !clip) return;
    lv_area_t area;
    lv_obj_get_coords(drawing_, &area);
    lv_color_t base = lv_color_hex(0xA4ECFF);
    lv_color_t dim = lv_color_hex(0x69B0C0);
    uint32_t duration = FaceData::faces[face_id_].cycle_ms;
    float phase = duration ? (millis()-face_at_) % duration / float(duration) : 0;
    const auto &face = FaceData::faces[face_id_];
    const auto *a = &FaceData::poses[face.pose_start];
    const auto *b = a;
    for (int i=1; i<face.pose_count; ++i) {
        b = &FaceData::poses[face.pose_start+i];
        if (phase*1000 <= b->t) break;
        a = b;
    }
    float mix = b->t == a->t ? 0 : smoother((phase*1000-a->t)/(b->t-a->t));
    auto lerp = [mix](float x, float y) { return x + (y-x)*mix; };
    float dx = lerp(a->x10,b->x10)/10, dy = lerp(a->y10,b->y10)/10;
    float rotation = lerp(a->r10,b->r10)/10 * 0.01745329f;
    float left_scale = lerp(a->ly,b->ly)/100;
    float right_scale = lerp(a->ry,b->ry)/100;
    float mouth_scale = lerp(a->my,b->my)/100;
    auto transform = [&](uint16_t index, const FaceData::Shape &shape) {
        const auto &src = FaceData::points[index];
        float center_y = shape.role == 2 ? 164 : 123;
        float scale_y = shape.role == 2 ? mouth_scale : src.x < 120 ? left_scale : right_scale;
        float x = src.x + dx, y = center_y + (src.y-center_y)*scale_y + dy;
        float rx = 120 + (x-120)*cosf(rotation) - (y-130)*sinf(rotation);
        float ry = 130 + (x-120)*sinf(rotation) + (y-130)*cosf(rotation);
        return lv_point_t{static_cast<lv_coord_t>(area.x1+roundf(rx)),
                          static_cast<lv_coord_t>(area.y1+roundf(ry))};
    };
    // LVGL's software rasterizer can spend several seconds painting a face
    // with many filled SVG paths. Yield from inside its draw callback so the
    // core-0 idle task can feed the task watchdog while we keep LVGL single-
    // threaded. No other task accesses LVGL objects.
    uint16_t primitives_since_yield = 0;
    const auto yield_during_render = [&]() {
        if (++primitives_since_yield >= 8) {
            primitives_since_yield = 0;
            vTaskDelay(1);
        }
    };
    lv_point_t transformed_points[64];
    float intersections[64];
    for (int si=0; si<face.shape_count; ++si) {
        const auto &shape = FaceData::shapes[face.shape_start+si];
        lv_color_t color = shape.color ? base : dim;
        if (shape.fill) {
            lv_draw_rect_dsc_t dsc;
            lv_draw_rect_dsc_init(&dsc);
            dsc.bg_color = color;
            dsc.bg_opa = LV_OPA_COVER;
            if (shape.count > 64) continue;
            int min_y = LV_COORD_MAX, max_y = LV_COORD_MIN;
            for (int point_i = 0; point_i < shape.count; ++point_i) {
                transformed_points[point_i] = transform(shape.start + point_i, shape);
                min_y = LV_MIN(min_y, transformed_points[point_i].y);
                max_y = LV_MAX(max_y, transformed_points[point_i].y);
            }
            min_y = LV_MAX(min_y, clip->y1);
            max_y = LV_MIN(max_y, clip->y2);
            for (int y = min_y; y <= max_y; ++y) {
                const float sample_y = y + .5f;
                uint8_t count = 0;
                for (int edge = 0; edge < shape.count; ++edge) {
                    const auto &a = transformed_points[edge];
                    const auto &b = transformed_points[(edge + 1) % shape.count];
                    if ((a.y <= sample_y && sample_y < b.y) ||
                        (b.y <= sample_y && sample_y < a.y)) {
                        intersections[count++] = a.x +
                            (sample_y - a.y) * (b.x - a.x) / (b.y - a.y);
                    }
                }
                for (uint8_t i = 1; i < count; ++i) {
                    const float value = intersections[i];
                    uint8_t j = i;
                    while (j > 0 && intersections[j - 1] > value) {
                        intersections[j] = intersections[j - 1];
                        --j;
                    }
                    intersections[j] = value;
                }
                for (uint8_t i = 0; i + 1 < count; i += 2) {
                    lv_area_t span = {
                        .x1 = static_cast<lv_coord_t>(ceilf(intersections[i] - .5f)),
                        .y1 = static_cast<lv_coord_t>(y),
                        .x2 = static_cast<lv_coord_t>(floorf(intersections[i + 1] - .5f)),
                        .y2 = static_cast<lv_coord_t>(y)
                    };
                    if (span.x2 >= span.x1) lv_draw_rect(&span, clip, &dsc);
                }
                if ((y & 7) == 7) yield_during_render();
            }
        } else {
            lv_draw_line_dsc_t dsc;
            lv_draw_line_dsc_init(&dsc);
            dsc.color = color;
            dsc.width = shape.width ? shape.width : 2;
            dsc.round_start = dsc.round_end = 1;
            for (int point_index=1; point_index<shape.count; ++point_index) {
                lv_point_t a = transform(shape.start+point_index-1,shape);
                lv_point_t b = transform(shape.start+point_index,shape);
                lv_draw_line(&a,&b,clip,&dsc);
                yield_during_render();
            }
        }
    }
    // V4.8 draws the aura on the left and right cheeks, centered at 180 and 0 degrees.
    static const uint32_t aura_colors[7] = {
        0xA4ECFF, 0xFFC084, 0x8CF3D4, 0xFF8DA0,
        0xFFA88F, 0xBBAAFF, 0xA2EED7
    };
    static const uint16_t aura_tempos[7] = {5400, 5200, 3600, 4900, 8100, 6200, 4200};
    static const float rest_opacity[7] = {.50f, .70f, .60f, .62f, .55f, .58f, .60f};
    int state_index = face_id_ >= 25 && face_id_ <= 31 ? face_id_ - 25 : -1;
    if (state_index < 0) return;
    lv_color_t aura_color = lv_color_hex(aura_colors[state_index]);
    uint32_t elapsed = millis() - face_at_;
    float cycle = (elapsed % aura_tempos[state_index]) / float(aura_tempos[state_index]);
    float wave = (1 - cosf(cycle * 6.2831853f)) * .5f;
    float energy = .48f + .25f * wave, sweep = .20f + .55f * cycle, span = 25;
    if (state_index == 0) { energy = .38f + .24f * wave; sweep = .13f + .70f * cycle; span = 22; }
    if (state_index == 1) {
        float b1 = pulse(fminf(1, elapsed / 1400.f), .04f, .16f);
        float b2 = pulse(fminf(1, elapsed / 1400.f), .27f, .17f);
        energy = .47f + .25f * wave + .33f * (b1 + b2);
        sweep = .30f + .50f * cycle; span = 19;
    }
    if (state_index == 2) {
        float burst = pulse(fminf(1, elapsed / 1700.f), .04f, .55f);
        energy = .36f + .20f * wave + .48f * burst;
        sweep = .20f + .65f * cycle; span = 20 + 16 * burst;
    }
    if (state_index == 3) {
        float fall = pulse(fminf(1, elapsed / 1300.f), .04f, .44f);
        energy = .35f + .20f * wave + .30f * fall;
        sweep = .52f - .25f * fall; span = 23;
    }
    if (state_index == 4) { energy = .29f + .18f * wave; sweep = .30f + .22f * cycle; span = 18; }
    if (state_index == 5) {
        float tap = pulse(cycle, .12f, .24f);
        energy = .38f + .20f * wave + .20f * tap;
        sweep = .08f + .67f * cycle; span = 18;
    }
    if (state_index == 6) {
        float ping = pulse(fminf(1, elapsed / 1300.f), .05f, .36f);
        energy = .34f + .24f * wave + .38f * ping;
        sweep = .15f + .72f * cycle; span = 22 + 10 * ping;
    }
    energy = fmaxf(0, fminf(1, energy));
    float enter = smoother(fminf(1, elapsed / 460.f));
    // Keep the status aura visible on the real 240 px LCD. The browser mockup
    // has a larger luminous surface; the original low alpha disappeared on
    // the small TFT, especially on its narrow edge arcs.
    const float rest_factor = .68f + .32f * rest_opacity[state_index];
    auto draw_arc = [&](float start, float end, lv_draw_arc_dsc_t &dsc) {
        // LVGL accepts angles in [0, 360]; split the right cheek at zero.
        if (end <= 0) {
            lv_draw_arc(area.x1+120, area.y1+120, 106, 360+start, 360+end, clip, &dsc);
            return;
        }
        if (start < 0) {
            lv_draw_arc(area.x1+120, area.y1+120, 106, 360+start, 360, clip, &dsc);
            start = 0;
        }
        if (end > start) lv_draw_arc(area.x1+120, area.y1+120, 106, start, end, clip, &dsc);
    };
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.color = aura_color;
    for (int side = 0; side < 2; ++side) {
        float a = side == 0 ? 146.f : -26.f;
        float b = side == 0 ? 206.f : 34.f;
        arc.width = 2;
        arc.opa = static_cast<lv_opa_t>(255 * (.42f + .14f * energy) * rest_factor * enter);
        draw_arc(a, b, arc);
        float side_phase = side == 0 ? sweep : 1 - sweep;
        float head = a + (b - a) * side_phase;
        float start = fmaxf(a, head - span * .55f);
        float end = fminf(b, head + span * .45f);
        arc.width = 4;
        arc.opa = static_cast<lv_opa_t>(255 * (.68f + .22f * energy) * rest_factor * enter);
        if (end > start) draw_arc(start, end, arc);
    }
}
