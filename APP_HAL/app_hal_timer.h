#ifndef APP_HAL_TIMER_H
#define APP_HAL_TIMER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { APP_HAL_TIMER_IDLE, APP_HAL_TIMER_RUNNING, APP_HAL_TIMER_PAUSED,
               APP_HAL_TIMER_FINISHED } app_hal_timer_state_t;
typedef struct {
    app_hal_timer_state_t state;
    uint32_t elapsed_ms, remaining_ms, lap_ms;
    uint8_t countdown, alarm_active, has_lap;
} app_hal_timer_status_t;

/* 注册计时功能。显示分.秒 00.00～99.59，分辨 1 秒；查询结果以 ms 表示。 */
app_status_t app_hal_timer_init(void);
app_status_t app_hal_timer_de_init(void);
/* 倒计时 1～5999000 ms；到时提醒保持 00.00 + RGB + 蜂鸣器，直到确认。
 * 重新开始会复位先前计时；duration_ms=0 使用配置头中的默认时长。 */
app_status_t app_hal_timer_countdown(uint32_t duration_ms, uint8_t alarm_on_finish);
app_status_t app_hal_timer_countup(void);
app_status_t app_hal_timer_pause(void);
app_status_t app_hal_timer_resume(void);
app_status_t app_hal_timer_stop(void);
/* 锁存从起跑或上一次计圈以来的本圈耗时，短暂显示后恢复总计时。 */
app_status_t app_hal_stopwatch_lap(uint32_t *lap_ms);
app_status_t app_hal_timer_ack(void);
app_status_t app_hal_timer_get(app_hal_timer_status_t *status);
/* 前台 K1 暂停/恢复，K2 计圈，K3 停止，K4 确认到时提醒。 */
app_status_t app_hal_timer_update(void);

#ifdef __cplusplus
}
#endif
#endif
