#ifndef APP_HAL_NOISE_H
#define APP_HAL_NOISE_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float current_db_spl, peak_db_spl;
    uint64_t exposure_ms;
    uint8_t valid, has_peak;
} app_hal_noise_state_t;
/* 累计达到阈值的时间(ms)和最高未计权 dB SPL。调用公共声学校准函数后才有有效结果。
 * 两次连续有效超限采样间的时间计入累计量，传感器故障或长时间未服务的间隔跳过。 */
app_status_t app_hal_noise_init(void);
/* 修改阈值后重新开始统计峰值及超限时长。 */
app_status_t app_hal_noise_set_threshold(float threshold_db_spl);
app_status_t app_hal_noise_reset(void);
app_status_t app_hal_noise_get(app_hal_noise_state_t *state);
app_status_t app_hal_noise_update(void);
app_status_t app_hal_noise_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
