#pragma once
typedef enum { ESP_SLEEP_WAKEUP_UNDEFINED=0, ESP_SLEEP_WAKEUP_EXT0=2 } esp_sleep_wakeup_cause_t;
#ifdef __cplusplus
extern "C" {
#endif
esp_sleep_wakeup_cause_t esp_sleep_get_wakeup_cause(void);
#ifdef __cplusplus
}
#endif
