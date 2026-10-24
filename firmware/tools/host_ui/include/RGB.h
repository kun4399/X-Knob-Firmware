#pragma once
#include <stdint.h>
enum StripMode { STRIP_OFF, STRIP_IDLE, STRIP_WORKING, STRIP_APPROVAL, STRIP_OFFLINE, STRIP_LIGHT_BRIGHTNESS, STRIP_LIGHT_CCT, STRIP_SCENES, STRIP_LOW_BATTERY };
void strip_set_state(StripMode,uint8_t,uint16_t,uint32_t);
