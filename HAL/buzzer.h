#ifndef BUZZER_H
#define BUZZER_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* task 中调用，占空比 0..100%，频率 100..10000 Hz。
 * duration_ms > 0 响到时间后返回；0 表示持续响。占空比 0 立即关闭。
 * 例：buzzer_beep(30, 2400, 120); */
int buzzer_beep(uint8_t duty_percent, uint32_t hz, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
#endif
