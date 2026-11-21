#ifndef APP_BUZZER_H
#define APP_BUZZER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化无源蜂鸣器，保持静音；使用 TIM2_CH3 / PA2。 */
app_status_t app_buzzer_init(void);
/* 停止输出并释放蜂鸣器。 */
app_status_t app_buzzer_de_init(void);
/* 持续响铃或切换音高。frequency_hz 为 100~10000 Hz，立即返回。 */
app_status_t app_buzzer_start(uint32_t frequency_hz);
/* 停止持续响铃或取消 repeat 序列，保留已初始化的设备。 */
app_status_t app_buzzer_stop(void);
/* 单次响铃。duration_ms 为 1~65535 ms；挂起调用线程，到时静音并返回。
 * 此期间其他蜂鸣器调用返回 BUSY。 */
app_status_t app_buzzer_beep(uint32_t frequency_hz, uint16_t duration_ms);
/* 异步重复鸣响，首响立即开始，最后一次响满后自动静音。frequency_hz 为 100~10000，
 * duty_percent 为 PWM 高电平百分比 0~100（0 为恒低、100 为恒高，发声选 1~99）。
 * on_ms 是每次鸣响时长（>0），gap_ms 是两响之间的静音时长（可为 0），count 为次数（>0）。
 * 时长向上取整到系统节拍，首段额外预留一拍补偿启动相位。1~99% 占空比且每响不超过一个
 * 载波周期时，序列临时独占 TIM2 的计数相位，与其他通道冲突返回 BUSY。更长响段可同频共享。
 * 序列中再次 start、beep、repeat 均返回 BUSY；调用 stop 或 de_init 可提前结束。 */
app_status_t app_buzzer_repeat(uint32_t frequency_hz, uint8_t duty_percent, uint32_t on_ms,
                               uint32_t gap_ms, uint32_t count);
#ifdef __cplusplus
}
#endif
#endif
