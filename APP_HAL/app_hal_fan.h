#ifndef APP_HAL_FAN_H
#define APP_HAL_FAN_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 使用集中配置的温度曲线启用 NTC 控风扇。NTC 读取硬错误时输出故障占空比。 */
app_status_t app_hal_fan_init(void);
/* 温度单位 ℃；stop < start < full。启动后在 start..full 之间线性调速。
 * 温度回落到 stop 时停转，中间回差区维持最低驱动。 */
app_status_t app_hal_fan_configure(float stop_c, float start_c, float full_c, uint8_t min_percent);
app_status_t app_hal_fan_get_power(uint8_t *percent);
app_status_t app_hal_fan_update(void);
app_status_t app_hal_fan_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
