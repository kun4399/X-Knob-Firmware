#ifndef __CONFIG_H__
#define __CONFIG_H__

#define SCLK 12
#define MOSI 11
#define TFT_CS 10
#define TFT_BLK 13
#define TFT_DC 14
#define TFT_RST 9

#define MO1 17
#define MO2 16
#define MO3 15

#define MT6701_SDA 1
#define MT6701_SCL 2
#define MT6701_SS 42

#define CONFIG_SCREEN_HOR_RES       240
#define CONFIG_SCREEN_VER_RES       240
#define CONFIG_SCREEN_BUFFER_SIZE   (CONFIG_SCREEN_HOR_RES * CONFIG_SCREEN_VER_RES /2)

#define PUSH_BUTTON_PIN 5
#define BATTERY_OFF_PIN 7
#define ON_OFF_PIN      18
#define LVGL_RUNNING_CORE   0
#define ESP32_RUNNING_CORE  1

// Default MQTT topic prefix used only when NVS is initialized for the first time.
#define MQTT_HOST               "home"

// Read optional private build settings before the public host placeholder.
// This lets deployment metadata live in the ignored header, never in Git.
#if __has_include("secrets.h")
#include "secrets.h"
#endif

// Match Mac Companion's XKNOB_MAC_ID via the ignored secrets.h override.
#ifndef XK_CODEX_HOST_ID
#define XK_CODEX_HOST_ID "example-host"
#endif


#define LCD_BK_DEFAULT_BRIGHTNESS  50
#define LCD_BK_DEFAULT_TIMEOUT     5


#endif
