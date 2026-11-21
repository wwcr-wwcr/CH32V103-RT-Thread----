#ifndef APP_HAL_CLOCK_H
#define APP_HAL_CLOCK_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 注册 RTC 和时钟页面，进入显示/校时后取得前台。 */
app_status_t app_hal_clock_init(void);
app_status_t app_hal_clock_de_init(void);
app_status_t app_hal_clock_show(void);
/* 编辑小时/分钟。K1 或 K2 增减，K3 从小时切到分钟、再次确认保存；K4 取消。
 * 保存保留当日日期并将秒清零。取消对 RTC 无写操作。 */
app_status_t app_hal_clock_edit_start(void);
app_status_t app_hal_clock_edit_finish(uint8_t save);
app_status_t app_hal_clock_update(void);

#ifdef __cplusplus
}
#endif
#endif
