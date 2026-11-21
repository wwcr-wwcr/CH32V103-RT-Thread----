#ifndef APP_HAL_PARAM_H
#define APP_HAL_PARAM_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int32_t minimum, maximum, step;
    uint8_t decimals;
} app_hal_param_range_t;
typedef enum {
    APP_HAL_PARAM_IDLE, APP_HAL_PARAM_EDITING, APP_HAL_PARAM_SAVED, APP_HAL_PARAM_CANCELLED
} app_hal_param_state_t;

/* 注册通用数值编辑器。数值均为定点整数，如 decimals=1 时 375 代表 37.5。 */
app_status_t app_hal_param_init(void);
app_status_t app_hal_param_de_init(void);
/* 传入当前值和范围，复制到编辑器；仅在应用读取已保存的值后应用设置。
 * K1/K2 加减，长按连续调整，K3 确认，K4 取消。 */
app_status_t app_hal_param_edit_start(int32_t initial, const app_hal_param_range_t *range);
app_status_t app_hal_param_edit_finish(uint8_t save);
/* 保存时返回新值，取消时返回进入编辑前的值。 */
app_status_t app_hal_param_get(app_hal_param_state_t *state, int32_t *value);
app_status_t app_hal_param_update(void);

#ifdef __cplusplus
}
#endif
#endif
