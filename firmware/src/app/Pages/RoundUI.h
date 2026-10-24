#pragma once

#include "lvgl.h"
#include <string.h>

LV_FONT_DECLARE(xknob_round_ui_20);
LV_FONT_DECLARE(xknob_round_ui_12);
LV_FONT_DECLARE(xknob_round_ui_28);

namespace RoundUI {
constexpr lv_coord_t kDiameter = 240;
// At y=196, a 240 px circle has only about 168 px of horizontal space.
constexpr lv_coord_t kLowerTextWidth = 150;
constexpr lv_coord_t kMiddleTextWidth = 190;
constexpr const lv_font_t *kCaptionFont = &xknob_round_ui_12;
constexpr const lv_font_t *kBodyFont = &xknob_round_ui_20;
constexpr const lv_font_t *kHeroFont = &xknob_round_ui_28;

inline lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font,
                       uint32_t color, lv_coord_t width, lv_coord_t top)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_size(obj, width, font->line_height + 3);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    lv_obj_align(obj, LV_ALIGN_TOP_MID, 0, top);
    return obj;
}

// Lay out a large number and its unit as a single centered group. Several of
// the compact Bahnschrift assets intentionally contain only digits, so units
// must use a font whose glyph table actually includes them.
inline void centered_pair(lv_obj_t *value, lv_obj_t *unit,
                          const char *value_text, const char *unit_text,
                          const lv_font_t *value_font, const lv_font_t *unit_font,
                          uint32_t value_color, uint32_t unit_color,
                          lv_coord_t center_y, lv_coord_t max_width = 240)
{
    lv_label_set_text(value, value_text);
    lv_label_set_text(unit, unit_text ? unit_text : "");
    lv_obj_set_style_text_font(value, value_font, 0);
    lv_obj_set_style_text_font(unit, unit_font, 0);
    lv_obj_set_style_text_color(value, lv_color_hex(value_color), 0);
    lv_obj_set_style_text_color(unit, lv_color_hex(unit_color), 0);
    lv_label_set_long_mode(value, LV_LABEL_LONG_CLIP);
    lv_label_set_long_mode(unit, LV_LABEL_LONG_CLIP);

    const lv_coord_t value_width = lv_txt_get_width(value_text,
        static_cast<uint32_t>(strlen(value_text)), value_font, 0, LV_TEXT_FLAG_NONE);
    const bool has_unit = unit_text && unit_text[0] != '\0';
    const lv_coord_t unit_width = has_unit ? lv_txt_get_width(unit_text,
        static_cast<uint32_t>(strlen(unit_text)), unit_font, 0, LV_TEXT_FLAG_NONE) : 0;
    const lv_coord_t gap = has_unit ? 4 : 0;
    const lv_coord_t total = value_width + gap + unit_width;
    const lv_coord_t left = (LV_HOR_RES - max_width) / 2 + (max_width - total) / 2;
    const lv_coord_t value_height = value_font->line_height + 2;
    const lv_coord_t unit_height = unit_font->line_height + 2;
    const lv_coord_t top = center_y - (value_height > unit_height ? value_height : unit_height) / 2;

    lv_obj_set_size(value, value_width + 2, value_height);
    lv_obj_set_size(unit, has_unit ? unit_width + 2 : 1, unit_height);
    lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_style_text_align(unit, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_align(value, LV_ALIGN_TOP_LEFT, left, top);
    lv_obj_align(unit, LV_ALIGN_TOP_LEFT, left + value_width + gap,
                 top + (value_height - unit_height) / 2);
    if (has_unit) lv_obj_clear_flag(unit, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(unit, LV_OBJ_FLAG_HIDDEN);
}
}
