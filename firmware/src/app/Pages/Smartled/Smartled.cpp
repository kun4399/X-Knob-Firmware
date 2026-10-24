#include "Smartled.h"
#include "../RoundUI.h"
#include "services/LightControl.h"
#include "hal/motor.h"
#include <Arduino.h>
#include <math.h>
#include <string.h>

using namespace Page;

void Smartled::onCustomAttrConfig() { SetCustomCacheEnable(false); }
void Smartled::onViewDidLoad() {}
void Smartled::onViewDidAppear() {}
void Smartled::onViewWillDisappear() {}

void Smartled::onViewLoad()
{
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x03080E), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

    ring = lv_arc_create(root);
    // Keep the value area clear and move the progress ring to the screen edge,
    // matching the V4.8 circular dial on the physical 240 px panel.
    lv_obj_set_size(ring, 234, 234);
    lv_obj_align(ring, LV_ALIGN_CENTER, 0, -1);
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_bg_angles(ring, 148, 32);
    lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ring, 7, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x203241), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 7, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);

    static const uint32_t temp_colors[] = {
        0xFFA971, 0xFFBC8C, 0xFFD0AD, 0xFFE9D7, 0xD8EAF4, 0xB3DDFC, 0x88CBFF
    };
    for (int i = 0; i < 7; ++i) {
        temperature_segments[i] = lv_arc_create(root);
        lv_obj_set_size(temperature_segments[i], 234, 234);
        lv_obj_align(temperature_segments[i], LV_ALIGN_CENTER, 0, -1);
        lv_arc_set_range(temperature_segments[i], 0, 100);
        const int start = (148 + i * 35) % 360;
        const int end = (start + 33) % 360;
        lv_arc_set_bg_angles(temperature_segments[i], start, end);
        lv_arc_set_value(temperature_segments[i], 0);
        lv_obj_remove_style(temperature_segments[i], nullptr, LV_PART_KNOB);
        lv_obj_clear_flag(temperature_segments[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_arc_width(temperature_segments[i], 7, LV_PART_MAIN);
        lv_obj_set_style_arc_color(temperature_segments[i], lv_color_hex(temp_colors[i]), LV_PART_MAIN);
        lv_obj_set_style_arc_opa(temperature_segments[i], LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_arc_width(temperature_segments[i], 0, LV_PART_INDICATOR);
        lv_obj_add_flag(temperature_segments[i], LV_OBJ_FLAG_HIDDEN);
    }

    temperature_knob = lv_obj_create(root);
    lv_obj_remove_style_all(temperature_knob);
    lv_obj_set_size(temperature_knob, 11, 11);
    lv_obj_set_style_radius(temperature_knob, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(temperature_knob, lv_color_hex(0xF6FAFF), 0);
    lv_obj_set_style_bg_opa(temperature_knob, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(temperature_knob, 1, 0);
    lv_obj_set_style_border_color(temperature_knob, lv_color_hex(0x080D13), 0);
    lv_obj_add_flag(temperature_knob, LV_OBJ_FLAG_HIDDEN);

    title = RoundUI::label(root, Resource.GetFont("bahnschrift_17"), 0x8296A7, 190, 55);
    lv_obj_set_style_text_letter_space(title, 1, 0);

    value_label = RoundUI::label(root, Resource.GetFont("bahnschrift_32"), 0xFFFFFF,
                                 200, 84);
    lv_obj_align(value_label, LV_ALIGN_TOP_MID, 0, 84);
    unit_label = RoundUI::label(root, &xknob_round_ui_20, 0xFFD4A0, 40, 84);
    category_label = RoundUI::label(root, &xknob_round_ui_12, 0x8296A7,
                                    RoundUI::kMiddleTextWidth, 164);

    status_label = RoundUI::label(root, &xknob_round_ui_12, 0x8296A7,
                                  RoundUI::kLowerTextWidth, 199);
    lv_obj_set_height(status_label, 34);
    lv_label_set_long_mode(status_label, LV_LABEL_LONG_WRAP);
    for (int i = 0; i < SceneStore::count; ++i) {
        scene_dots[i] = lv_obj_create(root);
        lv_obj_remove_style_all(scene_dots[i]);
        lv_obj_set_style_radius(scene_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(scene_dots[i], LV_OPA_COVER, 0);
        lv_obj_align(scene_dots[i], LV_ALIGN_CENTER, -18 + i * 12, 52);
        lv_obj_add_flag(scene_dots[i], LV_OBJ_FLAG_HIDDEN);
    }
    scenes_.load();
    refresh();
}

void Smartled::onViewWillAppear()
{
    HAL::mqtt_init();
    HAL::smartled_command("state");
    scenes_.load();
    selected_scene = SceneStore::selected();
    setMode(BRIGHTNESS);
    gesture_.suppress_pressed_until_release(HAL::encoder_is_pushed());
    gesture_.update(HAL::encoder_is_pushed(), HAL::get_motor_position(), millis());
    timer = lv_timer_create(onTimer, 25, this);
}

void Smartled::onViewDidDisappear()
{
    lv_timer_del(timer);
    timer = nullptr;
}

void Smartled::setMode(Mode next)
{
    mode = next;
    int motor_mode;
    int position;
    if (mode == BRIGHTNESS) {
        motor_mode = MOTOR_UNBOUND_BRIGHTNESS;
        position = desired_brightness;
    } else if (mode == TEMPERATURE) {
        motor_mode = MOTOR_UNBOUND_CCT;
        position = light_control_cct_detent_position(desired_temperature, 72, 2700, 6500);
    } else if (mode == POWER) {
        motor_mode = MOTOR_ON_OFF_STRONG_DETENTS;
        position = desired_power ? 1 : 0;
    } else {
        motor_mode = MOTOR_UNBOUND_COARSE_DETENTS;
        position = selected_scene;
    }
    HAL::update_motor_mode(motor_mode, position);
    gesture_.rebase(HAL::get_motor_position());
    ignore_motor_until = millis() + 150;
    refresh();
}

void Smartled::refresh()
{
    int arc_value = 0;
    lv_color_t accent;
    char formatted_value[12];
    lv_obj_set_style_text_font(title, Resource.GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_font(value_label, Resource.GetFont("bahnschrift_32"), 0);
    lv_obj_set_size(value_label, 200, 84);
    lv_obj_align(value_label, LV_ALIGN_TOP_MID, 0, 84);
    lv_obj_set_style_text_font(category_label, &xknob_round_ui_12, 0);
    lv_obj_align(category_label, LV_ALIGN_TOP_MID, 0, 164);
    for (auto *dot : scene_dots) lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    if (mode == BRIGHTNESS) {
        lv_label_set_text(title, "BRIGHTNESS");
        snprintf(formatted_value, sizeof(formatted_value), "%d", desired_brightness);
        RoundUI::centered_pair(value_label, unit_label, formatted_value, "%",
            Resource.GetFont("bahnschrift_65"), &xknob_round_ui_20, 0xF1FBFF, 0xFFD4A0, 112);
        lv_label_set_text(category_label, "亮 度");
        lv_obj_add_flag(temperature_knob, LV_OBJ_FLAG_HIDDEN);
        for (auto *segment : temperature_segments) lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_HIDDEN);
        arc_value = desired_brightness;
        accent = lv_color_hex(0xFFD4A0);
    } else if (mode == TEMPERATURE) {
        lv_label_set_text(title, "COLOR TEMP");
        snprintf(formatted_value, sizeof(formatted_value), "%d", desired_temperature);
        RoundUI::centered_pair(value_label, unit_label, formatted_value, "K",
            Resource.GetFont("bahnschrift_65"), &xknob_round_ui_20, 0xF1FBFF, 0x8DEBFF, 119);
        lv_label_set_text(category_label, desired_temperature < 3500 ? "暖 黄 白" :
            desired_temperature > 5000 ? "冷 白" : "中 性 白");
        lv_obj_add_flag(ring, LV_OBJ_FLAG_HIDDEN);
        for (auto *segment : temperature_segments) lv_obj_clear_flag(segment, LV_OBJ_FLAG_HIDDEN);
        float fraction = constrain(desired_temperature - 2700, 0, 3800) / 3800.0f;
        float angle = (148.0f + 244.0f * fraction) * 0.0174532925f;
        lv_obj_align(temperature_knob, LV_ALIGN_CENTER,
                     (lv_coord_t)lroundf(cosf(angle) * 117.0f),
                     (lv_coord_t)lroundf(sinf(angle) * 117.0f) - 1);
        lv_obj_clear_flag(temperature_knob, LV_OBJ_FLAG_HIDDEN);
        arc_value = (desired_temperature - 2700) * 100 / 3800;
        accent = lv_color_hex(0x8DEBFF);
    } else if (mode == POWER) {
        lv_label_set_text(title, "LIGHT POWER");
        RoundUI::centered_pair(value_label, unit_label, desired_power ? "开" : "关", "",
            &xknob_round_ui_28, &xknob_round_ui_20, 0x7BECC4, 0x7BECC4, 119);
        lv_label_set_text(category_label, desired_power ? "灯光已开启" : "灯光已关闭");
        lv_obj_add_flag(temperature_knob, LV_OBJ_FLAG_HIDDEN);
        for (auto *segment : temperature_segments) lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_HIDDEN);
        arc_value = desired_power ? 100 : 0;
        accent = lv_color_hex(0x7BECC4);
    } else {
        lv_obj_add_flag(unit_label, LV_OBJ_FLAG_HIDDEN);
        const LightScene scene = scenes_.get(selected_scene);
        lv_obj_set_style_text_font(title, &xknob_round_ui_20, 0);
        lv_label_set_text_fmt(title, "场景  %02d / %02d", selected_scene + 1, SceneStore::count);
        lv_label_set_text(value_label, SceneStore::name(selected_scene));
        lv_label_set_text_fmt(category_label, "%d%%  ·  %dK", scene.brightness, scene.temperature);
        lv_obj_set_style_text_font(value_label, &xknob_round_ui_28, 0);
        lv_obj_set_size(value_label, 190, 50);
        lv_obj_align(value_label, LV_ALIGN_TOP_MID, 0, 101);
        lv_obj_set_style_text_color(value_label, lv_color_hex(0xF1FBFF), 0);
        lv_obj_align(category_label, LV_ALIGN_TOP_MID, 0, 151);
        lv_obj_add_flag(temperature_knob, LV_OBJ_FLAG_HIDDEN);
        for (auto *segment : temperature_segments) lv_obj_add_flag(segment, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(ring, LV_OBJ_FLAG_HIDDEN);
        arc_value = scene.brightness;
        accent = lv_color_hex(0xFFBD76);
        const float pulse = .5f + .5f * sinf(millis() * .003f);
        for (int i = 0; i < SceneStore::count; ++i) {
            const bool selected = i == selected_scene;
            lv_obj_set_size(scene_dots[i], selected ? 7 : 4, selected ? 7 : 4);
            lv_obj_set_style_bg_color(scene_dots[i], lv_color_hex(selected ? 0xFFBD76 : 0x324555), 0);
            lv_obj_set_style_bg_opa(scene_dots[i], selected ? (lv_opa_t)(185 + pulse * 70) : LV_OPA_COVER, 0);
            lv_obj_clear_flag(scene_dots[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    lv_arc_set_value(ring, arc_value);
    lv_obj_set_style_arc_color(ring, accent, LV_PART_INDICATOR);
    lv_obj_align(status_label, LV_ALIGN_TOP_MID, 0, 199);
    const char *feedback = !state.online ? "连接断开" : failed ? "未确认" :
        (dirty || awaiting) ? "正在确认" : "已确认";
    lv_label_set_text(status_label, feedback);
    lv_obj_set_style_text_color(status_label, !state.online || failed ? lv_color_hex(0xFF8DA0) :
        dirty || awaiting ? lv_color_hex(0xFFD4A0) : lv_color_hex(0x8296A7), 0);
}

void Smartled::send()
{
    if (!state.online) { dirty = false; failed = true; refresh(); return; }
    const bool include_scene = scene_apply_dirty;
    const bool include_power = include_scene || mode == POWER;
    const bool include_brightness = include_scene || mode == BRIGHTNESS;
    const bool include_temperature = include_scene || mode == TEMPERATURE;
    if (HAL::smartled_set(include_power, desired_power,
                          include_brightness, desired_brightness,
                          include_temperature, desired_temperature,
                          pending_id, sizeof(pending_id))) {
        sent_ms = millis();
        awaiting = true;
        failed = false;
        pending_mode = mode;
        pending_value = mode == BRIGHTNESS ? desired_brightness :
                        mode == TEMPERATURE ? desired_temperature : desired_power;
        pending_scene = include_scene;
        scene_apply_dirty = false;
        pending_power = desired_power;
        pending_brightness = desired_brightness;
        pending_temperature = desired_temperature;
    } else {
        failed = true;
        scene_apply_dirty = false;
        pending_scene = false;
    }
    dirty = false;
    refresh();
}

void Smartled::update()
{
    // All DARK rotation belongs exclusively to the low-power direction switch.
    // Do not interpret it as menu/scene/task detents while LCD is off.
    if (HAL::power_is_dark()) {
        dirty = awaiting = scene_apply_dirty = pending_scene = false;
        return;
    }
    if (HAL::power_home_requested()) return;
    const unsigned long now = millis();
    if (HAL::take_wake_press()) gesture_.consume_wake_press(HAL::encoder_is_pushed());
    GestureFSM::Event gesture;
    const int motor_position = HAL::get_motor_position();
    if (static_cast<int32_t>(now - ignore_motor_until) < 0) {
        gesture_.rebase(motor_position);
    } else {
        gesture = gesture_.update(HAL::encoder_is_pushed(), motor_position, now);
    }
    if (gesture.kind == GestureFSM::LONG_PRESS) {
        if (mode == SCENES) {
            if (dirty) send();
            HAL::motor_exit_feedback();
            Manager->Pop();
        } else {
            if (dirty) send();
            setMode(SCENES);
        }
        return;
    }
    if (gesture.kind == GestureFSM::DOUBLE_CLICK) {
        // The light app is pushed from the carousel; close only this app so
        // the user returns to the same menu selection.
        Manager->Pop();
        return;
    }
    if (gesture.kind == GestureFSM::SINGLE_CLICK) {
        if (mode == SCENES) {
            const LightScene scene = scenes_.get(selected_scene);
            desired_brightness = scene.brightness;
            desired_temperature = scene.temperature;
            desired_power = true;
            scene_apply_dirty = true;
            setMode(BRIGHTNESS);
            changed_ms = now - 180;
            dirty = true;
            failed = false;
        } else {
            // In the light app, a click only selects the next control view.
            // Power is changed deliberately by rotating while POWER is shown.
            setMode(static_cast<Mode>(light_control_next_view(static_cast<int>(mode))));
        }
    } else if (gesture.kind == GestureFSM::ROTATE ||
               gesture.kind == GestureFSM::HOLD_ROTATE) {
        if (mode == SCENES) {
            selected_scene = (selected_scene + gesture.delta % SceneStore::count +
                              SceneStore::count) % SceneStore::count;
            SceneStore::select(selected_scene);
            if (scene_apply_dirty) {
                scene_apply_dirty = false;
                dirty = false;
            }
            HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, selected_scene);
            gesture_.rebase(HAL::get_motor_position());
            ignore_motor_until = now + 150;
            failed = false;
        } else if (mode == TEMPERATURE) {
            desired_temperature = light_control_apply_cct_detents(
                desired_temperature, gesture.delta, 72, 2700, 6500);
            changed_ms = now;
            dirty = true;
            failed = false;
        } else if (mode == BRIGHTNESS) {
            desired_brightness = light_control_apply_delta(desired_brightness, gesture.delta, 1, 100);
            changed_ms = now;
            dirty = true;
            failed = false;
        } else if (mode == POWER) {
            // Direction is an explicit value, not a toggle: clockwise means
            // ON and counterclockwise means OFF. Repeated detents are idempotent.
            const bool next_power = light_control_power_for_delta(desired_power, gesture.delta);
            if (desired_power != next_power) {
                desired_power = next_power;
                scene_apply_dirty = false;
                changed_ms = now;
                dirty = true;
                failed = false;
            }
        }
    }
    HAL::SmartledState received;
    if (HAL::smartled_state(&received)) {
        state = received;
        if (!received.online && (dirty || awaiting)) {
            dirty = awaiting = scene_apply_dirty = pending_scene = false;
            failed = true;
        }
        if (received.online && received.received_ms != last_state_ms) {
            last_state_ms = received.received_ms;
            if (!dirty && !awaiting) {
                const bool brightness_changed = desired_brightness != received.brightness;
                const bool temperature_changed = desired_temperature != received.temperature;
                const bool power_changed = desired_power != received.power;
                desired_brightness = received.brightness;
                desired_temperature = received.temperature;
                desired_power = received.power;
                if ((mode == BRIGHTNESS && brightness_changed) ||
                    (mode == TEMPERATURE && temperature_changed) ||
                    (mode == POWER && power_changed)) setMode(mode);
            }
        }
        const bool modern_ack = received.modern && received.ack_received_ms >= sent_ms &&
                                strcmp(received.last_ack_id, pending_id) == 0;
        const bool readback = light_control_readback_for_command(
            received.modern, received.online, received.ok,
            received.last_command_id, pending_id, received.received_ms, sent_ms);
        if (awaiting && readback) {
            const bool matched = pending_scene ?
                received.power == pending_power && received.brightness == pending_brightness &&
                    received.temperature == pending_temperature :
                pending_mode == BRIGHTNESS ? received.brightness == pending_value :
                pending_mode == TEMPERATURE ? received.temperature == pending_value :
                received.power == (pending_value != 0);
            if (matched && received.ok) {
                awaiting = false;
                failed = false;
                retries = 0;
                pending_scene = false;
            }
        }
        if (awaiting && modern_ack && !received.last_ack_success) {
            awaiting = false;
            failed = true;
            pending_scene = false;
        }
    }
    // Coalesce rapid encoder steps so one settled value produces one MQTT command.
    if (dirty && now - changed_ms >= 180) send();
    if (awaiting && now - sent_ms > 7000) {
        awaiting = false;
        pending_scene = false;
        failed = true;
    }
    refresh();
}

void Smartled::onTimer(lv_timer_t *timer)
{
    static_cast<Smartled *>(timer->user_data)->update();
}
