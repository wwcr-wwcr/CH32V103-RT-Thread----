#ifndef APP_HAL_RPM_H
#define APP_HAL_RPM_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    float rpm;          /* 非负转速；方向由 direction 返回 */
    int8_t direction;   /* -1 反转，0 静止，1 正转；正向随 A/B 接线确定 */
    int64_t position;   /* 累计四倍频计数 */
} app_hal_rpm_sample_t;
/* counts_per_rev 是被测轴转一整转的四倍频计数，包括减速比。首次完整时间窗后可读取。
 * 服务频率须保证两次编码器读取之间的净位移小于 32768 计数。 */
app_status_t app_hal_rpm_init(uint32_t counts_per_rev);
app_status_t app_hal_rpm_read(app_hal_rpm_sample_t *sample);
/* 请求显示带方向的整数 RPM；超过四位显示 ----。 */
app_status_t app_hal_rpm_show(void);
app_status_t app_hal_rpm_update(void);
app_status_t app_hal_rpm_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
