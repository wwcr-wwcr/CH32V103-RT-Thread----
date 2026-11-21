#ifndef APP_OUTPUT_PATTERN_H
#define APP_OUTPUT_PATTERN_H
#include "app_internal.h"

/* 蜂鸣器和 RGB 各有独立的两相输出和定时器。level=0 为关闭。 */
typedef struct {
    uint16_t level;
    rt_tick_t ticks;
} app_output_phase_t;

typedef struct {
    struct rt_timer timer;
    app_status_t (*write)(uint16_t level);
    void (*done)(void);
    app_output_phase_t phase[2];
    volatile uint32_t remaining;
    uint8_t index, stopping;
} app_output_pattern_t;

/* 检查毫秒值并向上取整到 RT tick。第一段必须非零，第二段允许为零。 */
int app_output_durations(uint32_t first_ms, uint32_t second_ms,
                         app_output_phase_t *first, app_output_phase_t *second);
void app_output_pattern_init(app_output_pattern_t *pattern, const char *name,
                             app_status_t (*write)(uint16_t), void (*done)(void));
/* 调用任务须持有设备锁并确认 remaining 为零。 */
app_status_t app_output_pattern_start(app_output_pattern_t *pattern, app_output_phase_t first,
                                      app_output_phase_t second, uint32_t count);
/* 设置固定输出，关闭失败时保留忙状态并逐拍重试。 */
app_status_t app_output_pattern_set(app_output_pattern_t *pattern, uint16_t level);
void app_output_pattern_release(app_output_pattern_t *pattern);
#endif
