#ifndef APP_HAL_ALARM_H
#define APP_HAL_ALARM_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_HAL_ALARM_TEMPERATURE, APP_HAL_ALARM_LIGHT, APP_HAL_ALARM_SOUND,
    APP_HAL_ALARM_DISTANCE, APP_HAL_ALARM_SOURCE_COUNT
} app_hal_alarm_source_t;
typedef struct {
    float value;
    uint8_t valid, triggered, acknowledged;
} app_hal_alarm_state_t;

/* 开启多源报警管理。按需配置各路；参数的单位分别为 ℃、lx、dB SPL、mm。 */
app_status_t app_hal_alarm_init(void);
/* trip > reset 为上限报警，trip < reset 为下限报警。连续越限 confirm_ms 后触发。
 * confirm_ms=0 表示一帧触发；跨越复位值即解除，调整本路设置会清除其旧事件。 */
app_status_t app_hal_alarm_configure(app_hal_alarm_source_t source, float trip, float reset,
                                     uint32_t confirm_ms);
/* 采用 app_hal_config.h 中的本路默认参数。声压报警须先校准声压。 */
app_status_t app_hal_alarm_enable_default(app_hal_alarm_source_t source);
app_status_t app_hal_alarm_disable(app_hal_alarm_source_t source);
/* 确认当次报警，保留视觉提示。恢复后再次越限可重新发声。
 * 阈值告警在前台时，短按 K4 也可确认正在显示的来源；其他来源仍可独立鸣响。 */
app_status_t app_hal_alarm_ack(app_hal_alarm_source_t source);
app_status_t app_hal_alarm_get(app_hal_alarm_source_t source, app_hal_alarm_state_t *state);
app_status_t app_hal_alarm_update(void);
app_status_t app_hal_alarm_de_init(void);

#ifdef __cplusplus
}
#endif
#endif
