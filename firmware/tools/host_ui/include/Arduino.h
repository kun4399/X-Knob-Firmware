#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
uint32_t millis(void);
void vTaskDelay(uint32_t ticks);
uint32_t esp_random(void);
#ifdef __cplusplus
}
template <typename T, typename L, typename H> inline T constrain(T v,L lo,H hi){return v<(T)lo?(T)lo:v>(T)hi?(T)hi:v;}
#endif
