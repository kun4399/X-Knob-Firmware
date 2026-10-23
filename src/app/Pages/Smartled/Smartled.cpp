#include "Smartled.h"
#include "hal/motor.h"
#include <Arduino.h>

using namespace Page;

void Smartled::onCustomAttrConfig() { SetCustomCacheEnable(false); }
void Smartled::onViewDidLoad() {}
void Smartled::onViewDidAppear() {}
void Smartled::onViewWillDisappear() {}

void Smartled::onViewLoad()
{
    lv_obj_remove_style_all(root);
    lv_obj_set_size(root, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_style_bg_color(root, lv_color_hex(0x0B1020), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_add_flag(root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(root, onEvent, LV_EVENT_ALL, this);

    ring = lv_arc_create(root);
    lv_obj_set_size(ring, 220, 220);
    lv_obj_center(ring);
    lv_arc_set_range(ring, 0, 100);
    lv_arc_set_bg_angles(ring, 135, 45);
    lv_obj_remove_style(ring, nullptr, LV_PART_KNOB);
    lv_obj_clear_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_width(ring, 12, LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring, lv_color_hex(0x242D45), LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring, 14, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring, true, LV_PART_INDICATOR);

    title = lv_label_create(root);
    lv_obj_set_style_text_font(title, Resource.GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0xA8B9D4), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -54);

    value_label = lv_label_create(root);
    lv_obj_set_style_text_font(value_label, Resource.GetFont("bahnschrift_65"), 0);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, -8);

    unit_label = lv_label_create(root);
    lv_obj_set_style_text_font(unit_label, Resource.GetFont("bahnschrift_17"), 0);
    lv_obj_set_style_text_color(unit_label, lv_color_hex(0xA8B9D4), 0);
    lv_obj_align(unit_label, LV_ALIGN_CENTER, 0, 43);

    status_label = lv_label_create(root);
    lv_obj_set_style_text_font(status_label, Resource.GetFont("bahnschrift_13"), 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(0x69C2B1), 0);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, 76);

    group = lv_group_create();
    lv_group_add_obj(group, root);
    lv_indev_set_group(lv_get_indev(LV_INDEV_TYPE_ENCODER), group);
    refresh();
}

void Smartled::onViewWillAppear()
{
    HAL::mqtt_init();
    HAL::smartled_command("state");
    setMode(BRIGHTNESS);
    timer = lv_timer_create(onTimer, 25, this);
}

void Smartled::onViewDidDisappear()
{
    lv_timer_del(timer);
    timer = nullptr;
}

void Smartled::onViewDidUnload()
{
    lv_group_del(group);
    group = nullptr;
}

void Smartled::setMode(Mode next)
{
    mode = next;
    int motor_mode;
    int position;
    if (mode == BRIGHTNESS) {
        motor_mode = MOTOR_BOUND_LCD_BK_BRIGHTNESS;
        position = desired_brightness;
    } else if (mode == TEMPERATURE) {
        motor_mode = MOTOR_BOUND_LCD_BK_TIMEOUT;
        position = (desired_temperature - 2700) * 30 / 3800;
    } else {
        motor_mode = MOTOR_ON_OFF_STRONG_DETENTS;
        position = desired_power ? 1 : 0;
    }
    HAL::update_motor_mode(motor_mode, position);
    last_position = position;
    refresh();
}

void Smartled::refresh()
{
    int arc_value = 0;
    lv_color_t accent;
    if (mode == BRIGHTNESS) {
        lv_label_set_text(title, "BRIGHTNESS");
        lv_label_set_text_fmt(value_label, "%d", desired_brightness);
        lv_label_set_text(unit_label, "%  |  press: warmth");
        arc_value = desired_brightness;
        accent = lv_color_hex(0xF5B85C);
    } else if (mode == TEMPERATURE) {
        lv_label_set_text(title, "WARMTH");
        lv_label_set_text_fmt(value_label, "%d", desired_temperature);
        lv_label_set_text(unit_label, "K  |  press: power");
        arc_value = (desired_temperature - 2700) * 100 / 3800;
        accent = lv_color_hex(0x8ABEFF);
    } else {
        lv_label_set_text(title, "LIGHT POWER");
        lv_label_set_text(value_label, desired_power ? "ON" : "OFF");
        lv_label_set_text(unit_label, "press: brightness");
        arc_value = desired_power ? 100 : 0;
        accent = lv_color_hex(0x77D4AA);
    }
    lv_arc_set_value(ring, arc_value);
    lv_obj_set_style_arc_color(ring, accent, LV_PART_INDICATOR);
    lv_obj_set_style_text_color(value_label, accent, 0);
    const char *status = !state.online ? "OFFLINE / waiting" :
                         failed ? "NOT CONFIRMED" :
                         (dirty || awaiting) ? "SENDING / verifying" :
                         "SYNCED / hold to exit";
    lv_label_set_text(status_label, status);
    lv_obj_set_style_text_color(status_label,
        !state.online || failed ? lv_color_hex(0xEC8090) : lv_color_hex(0x69C2B1), 0);
}

void Smartled::send()
{
    if (!state.online) { dirty = false; failed = true; refresh(); return; }
    char command[36];
    if (mode == BRIGHTNESS) snprintf(command, sizeof(command), "brightness %d", desired_brightness);
    else if (mode == TEMPERATURE) snprintf(command, sizeof(command), "temperature %d", desired_temperature);
    else snprintf(command, sizeof(command), "power %d", desired_power ? 1 : 0);
    if (HAL::smartled_command(command)) {
        sent_ms = millis();
        awaiting = true;
        failed = false;
        pending_mode = mode;
        pending_value = mode == BRIGHTNESS ? desired_brightness :
                        mode == TEMPERATURE ? desired_temperature : desired_power;
    } else {
        failed = true;
    }
    dirty = false;
    refresh();
}

void Smartled::update()
{
    HAL::SmartledState received;
    if (HAL::smartled_state(&received)) {
        state = received;
        if (received.online && received.received_ms != last_state_ms) {
            last_state_ms = received.received_ms;
            if (!dirty && !awaiting) {
                desired_brightness = received.brightness;
                desired_temperature = received.temperature;
                desired_power = received.power;
                setMode(mode);
            } else if (awaiting && received.received_ms >= sent_ms) {
                bool matched = pending_mode == BRIGHTNESS ? received.brightness == pending_value :
                               pending_mode == TEMPERATURE ? received.temperature == pending_value :
                               received.power == (pending_value != 0);
                if (matched || !received.ok) {
                    awaiting = false;
                    failed = !matched;
                }
            }
        }
    }
    int position = HAL::get_motor_position();
    if (position != last_position) {
        last_position = position;
        if (mode == BRIGHTNESS) desired_brightness = constrain(position, 1, 100);
        else if (mode == TEMPERATURE) desired_temperature = 2700 + constrain(position, 0, 30) * 3800 / 30;
        else desired_power = position != 0;
        changed_ms = millis();
        dirty = true;
        failed = false;
    }
    // Coalesce rapid encoder steps so one settled value produces one MQTT command.
    if (dirty && millis() - changed_ms >= 180) send();
    if (awaiting && millis() - sent_ms > 7000) { awaiting = false; failed = true; }
    refresh();
}

void Smartled::onTimer(lv_timer_t *timer)
{
    static_cast<Smartled *>(timer->user_data)->update();
}

void Smartled::onEvent(lv_event_t *event)
{
    auto *self = static_cast<Smartled *>(lv_event_get_user_data(event));
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_SHORT_CLICKED) {
        if (self->dirty) self->send();
        self->setMode(static_cast<Mode>((self->mode + 1) % 3));
    } else if (code == LV_EVENT_LONG_PRESSED) {
        if (self->dirty) self->send();
        HAL::update_motor_mode(MOTOR_UNBOUND_COARSE_DETENTS, 0);
        self->Manager->Pop();
    }
}
