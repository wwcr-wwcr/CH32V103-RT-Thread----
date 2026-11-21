#ifndef APP_HAL_COUNTER_H
#define APP_HAL_COUNTER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t minimum, maximum, initial, step, target;
    uint8_t wrap, notify_target;
} app_hal_counter_config_t;

/* 注册计数器。start(NULL) 采用配置头的范围、步长、目标值。 */
app_status_t app_hal_counter_init(void);
app_status_t app_hal_counter_de_init(void);
/* K1/K2 加减，长按连调；K3 恢复起点。范围须能在四位数码管显示。 */
app_status_t app_hal_counter_start(const app_hal_counter_config_t *config);
/* direction 为 +1/-1，按设置步长走一次；首次达到或越过目标触发一次提示。 */
app_status_t app_hal_counter_step(int8_t direction);
app_status_t app_hal_counter_set_target(int32_t target);
app_status_t app_hal_counter_get(int32_t *value);
app_status_t app_hal_counter_update(void);

#ifdef __cplusplus
}
#endif
#endif
