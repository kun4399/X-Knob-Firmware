#include "SceneEdit.h"
#include "../RoundUI.h"
#include "services/LightControl.h"
#include "services/SceneEditorLogic.h"
#include "hal/hal.h"
#include "hal/motor.h"
#include <math.h>

using namespace Page;

namespace {
constexpr uint32_t kAmber = 0xFFCD95;
constexpr uint32_t kBlue = 0x9EDAFF;
constexpr uint32_t kSceneAccent = 0x8DEBFF;

lv_obj_t *make_label(lv_obj_t *root, const lv_font_t *font, uint32_t color,
                     lv_coord_t width, lv_coord_t top, lv_coord_t x,
                     lv_text_align_t align) {
    lv_obj_t *obj = lv_label_create(root);
    lv_obj_set_size(obj, width, font->line_height + 4);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(obj, align, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, x, top);
    return obj;
}
}

void SceneEdit::onCustomAttrConfig() { SetCustomCacheEnable(false); }

void SceneEdit::onViewLoad() {
    scenes_.load();
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, 240, 240);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    title_ = RoundUI::label(root, &xknob_round_ui_20, 0x8C9DAC, 190, 22);
    scene_card_ = lv_obj_create(root);
    lv_obj_remove_style_all(scene_card_);
    lv_obj_set_size(scene_card_, 176, 38);
    lv_obj_align(scene_card_, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_set_style_radius(scene_card_, 10, 0);
    lv_obj_set_style_bg_color(scene_card_, lv_color_hex(0x101922), 0);
    lv_obj_set_style_bg_opa(scene_card_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scene_card_, 1, 0);
    lv_obj_set_style_border_color(scene_card_, lv_color_hex(0xFFCD95), 0);
    scene_name_ = RoundUI::label(root, &xknob_round_ui_20, 0xF1FBFF, 160, 52);
    save_status_ = RoundUI::label(root, &xknob_round_ui_12, 0x8296A7,
                                  RoundUI::kLowerTextWidth, 190);

    for (int i = 0; i < 2; ++i) {
        rows_[i] = lv_obj_create(root);
        lv_obj_remove_style_all(rows_[i]);
        lv_obj_set_size(rows_[i], 152, 40);
        lv_obj_align(rows_[i], LV_ALIGN_TOP_MID, 0, 94 + i * 46);
        lv_obj_set_style_radius(rows_[i], 10, 0);
        lv_obj_set_style_bg_color(rows_[i], lv_color_hex(0x101922), 0);
        lv_obj_set_style_bg_opa(rows_[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(rows_[i], 1, 0);
        lv_obj_set_style_border_color(rows_[i], lv_color_hex(0x243542), 0);

        field_labels_[i] = make_label(root, &xknob_round_ui_20, 0x9DB0BC, 68,
                                      102 + i * 46, -35, LV_TEXT_ALIGN_LEFT);
        value_labels_[i] = make_label(root, &xknob_round_ui_20, i == 0 ? kAmber : kBlue,
                                      74, 102 + i * 46, 35, LV_TEXT_ALIGN_RIGHT);
    }
    lv_label_set_text(field_labels_[0], "亮度");
    lv_label_set_text(field_labels_[1], "色温");
    for (int i = 0; i < SceneStore::count; ++i) working_[i] = scenes_.get(i);

    group_ = lv_group_create();
    lv_group_add_obj(group_, root);
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group_);
    render(millis());
}

void SceneEdit::onViewWillAppear() {
    scenes_.load();
    selected_ = SceneStore::selected();
    for (int i = 0; i < SceneStore::count; ++i) {
        working_[i] = scenes_.get(i);
        modified_[i] = false;
    }
    const LightScene scene = working_[selected_];
    brightness_ = scene.brightness;
    temperature_ = scene.temperature;
    selected_field_ = 0; // Scene selector is the first focus when the editor opens.
    save_pending_ = false;
    save_failed_ = false;
    saved_until_ = 0;
    HAL::update_motor_mode(MOTOR_UNBOUND_APP_MENU_DETENTS, selected_);
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis());
    timer_ = lv_timer_create(timer_cb, 33, this);
}

void SceneEdit::onViewDidDisappear() { if (timer_) { lv_timer_del(timer_); timer_ = nullptr; } }
void SceneEdit::onViewDidUnload() { if (group_) { lv_group_del(group_); group_ = nullptr; } }
void SceneEdit::timer_cb(lv_timer_t *t) { static_cast<SceneEdit *>(t->user_data)->update(); }

void SceneEdit::render(uint32_t now) {
    const char *title = selected_field_ == 0 ? "场景选择" :
                        selected_field_ == 1 ? "亮度编辑" : "色温编辑";
    lv_label_set_text(title_, save_failed_ ? "保存失败" : title);
    lv_label_set_text(scene_name_, SceneStore::name(selected_));
    const float pulse = .85f + .15f * sinf(now * .0042f);
    lv_obj_set_style_border_color(scene_card_, lv_color_hex(
        selected_field_ == 0 ? kSceneAccent : 0x243542), 0);
    lv_obj_set_style_border_width(scene_card_, selected_field_ == 0 ? 2 : 1, 0);
    lv_obj_set_style_border_opa(scene_card_, selected_field_ == 0 ?
        (lv_opa_t)(205 + pulse * 50) : LV_OPA_COVER, 0);
    for (int i = 0; i < 2; ++i) {
        const bool selected = i + 1 == selected_field_;
        const uint32_t accent = i == 0 ? kAmber : kBlue;
        lv_obj_set_style_border_color(rows_[i], lv_color_hex(selected ? accent : 0x243542), 0);
        lv_obj_set_style_border_width(rows_[i], selected ? 2 : 1, 0);
        lv_obj_set_style_border_opa(rows_[i], selected ? (lv_opa_t)(210 + pulse * 45) : LV_OPA_COVER, 0);
        lv_label_set_text_fmt(value_labels_[i], i == 0 ? "%d%%" : "%dK",
                              i == 0 ? brightness_ : temperature_);
    }
    const char *save_text = save_failed_ ? "保存失败，正在重试" :
                            save_pending_ ? "修改待自动保存" :
                            !scene_editor_deadline_reached(now, saved_until_) ? "已自动保存" : "";
    lv_label_set_text(save_status_, save_text);
    lv_obj_set_style_text_color(save_status_, lv_color_hex(save_failed_ ? 0xFF8DA0 :
        save_pending_ ? kAmber : 0x8296A7), 0);
}

bool SceneEdit::save_modified(uint32_t now) {
    bool saved = true;
    for (int i = 0; i < SceneStore::count; ++i) {
        if (!modified_[i]) continue;
        if (scenes_.update(i, working_[i].brightness, working_[i].temperature)) {
            modified_[i] = false;
        } else {
            saved = false;
        }
    }
    save_pending_ = !saved;
    save_failed_ = !saved;
    if (saved) {
        saved_until_ = now + 1200;
    } else {
        save_due_at_ = now + 1000;
    }
    return saved;
}

void SceneEdit::set_focus(int focus) {
    selected_field_ = focus;
    const int motor_mode = focus == 0 ? MOTOR_UNBOUND_APP_MENU_DETENTS :
                           focus == 1 ? MOTOR_UNBOUND_FINE_DETENTS : MOTOR_UNBOUND_CCT;
    const int position = focus == 0 ? selected_ : focus == 1 ? brightness_ :
        light_control_cct_detent_position(temperature_, 72, 2700, 6500);
    HAL::update_motor_mode(motor_mode, position);
    gesture_.rebase(HAL::get_motor_position());
}

void SceneEdit::select_scene(int delta) {
    selected_ = scene_editor_apply_delta(selected_, delta, SceneStore::count);
    SceneStore::select(selected_);
    brightness_ = working_[selected_].brightness;
    temperature_ = working_[selected_].temperature;
}

void SceneEdit::update() {
    const uint32_t now = millis();
    if (HAL::take_wake_press()) gesture_.consume_wake_press(HAL::encoder_is_pushed());
    const auto e = gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), now);
    if (e.kind == GestureFSM::DOUBLE_CLICK) {
        if (save_pending_ && !save_modified(now)) { render(now); return; }
        Manager->BackHome();
        return;
    }
    if (e.kind == GestureFSM::LONG_PRESS) {
        if (save_pending_ && !save_modified(now)) { render(now); return; }
        HAL::motor_exit_feedback();
        Manager->Pop();
        return;
    }
    if (e.kind == GestureFSM::ROTATE || e.kind == GestureFSM::HOLD_ROTATE) {
        if (selected_field_ == 0) {
            select_scene(e.delta);
        } else if (selected_field_ == 1) {
            brightness_ = constrain(brightness_ + e.delta, 1, 100);
            working_[selected_].brightness = brightness_;
            modified_[selected_] = true;
            save_pending_ = true;
            save_due_at_ = now + kSceneEditorSaveDebounceMs;
            save_failed_ = false;
            saved_until_ = 0;
        } else if (selected_field_ == 2) {
            temperature_ = light_control_apply_cct_detents(temperature_, e.delta,
                                                            72, 2700, 6500);
            working_[selected_].temperature = temperature_;
            modified_[selected_] = true;
            save_pending_ = true;
            save_due_at_ = now + kSceneEditorSaveDebounceMs;
            save_failed_ = false;
            saved_until_ = 0;
        }
    }
    if (e.kind == GestureFSM::SINGLE_CLICK) {
        set_focus(scene_editor_next_focus(selected_field_));
    }
    if (scene_editor_save_due(save_pending_, now, save_due_at_))
        save_modified(now);
    render(now);
}
