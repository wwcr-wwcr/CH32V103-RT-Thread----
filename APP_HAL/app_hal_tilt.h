#ifndef APP_HAL_TILT_H
#define APP_HAL_TILT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float degrees;
    uint8_t valid, alarm;
} app_hal_tilt_state_t;
/* 接入 QMI8658。+Z 朝上为 0°，+Z 水平为 90°，+Z 朝下为 180°。
 * 使用加速度大小、角速度筛除运动帧；输出适合静止或缓慢运动。 */
app_status_t app_hal_tilt_init(void);
app_status_t app_hal_tilt_configure(float trip_deg, float reset_deg, uint32_t confirm_ms);
app_status_t app_hal_tilt_get(app_hal_tilt_state_t *state);
app_status_t app_hal_tilt_show(void);
app_status_t app_hal_tilt_update(void);
app_status_t app_hal_tilt_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
