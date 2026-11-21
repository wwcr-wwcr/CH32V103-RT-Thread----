#ifndef APP_HAL_PROXIMITY_H
#define APP_HAL_PROXIMITY_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    uint16_t distance_mm;
    uint32_t passages;
    uint8_t valid, occupied;
} app_hal_proximity_state_t;
/* 默认采用集中配置的近、远距离及鸣叫间隔。进入近区计一次，退出远区才重新布防；
 * 无回波不算离开，计数达到 UINT32_MAX 后保持。 */
app_status_t app_hal_proximity_init(void);
app_status_t app_hal_proximity_configure(uint16_t near_mm, uint16_t far_mm);
app_status_t app_hal_proximity_get(app_hal_proximity_state_t *state);
app_status_t app_hal_proximity_reset_count(void);
/* 请求显示焦点：0 显示距离(mm)，1 显示经过次数。超过四位时显示 ----。 */
app_status_t app_hal_proximity_show(uint8_t show_count);
app_status_t app_hal_proximity_update(void);
app_status_t app_hal_proximity_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
