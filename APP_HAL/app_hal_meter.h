#ifndef APP_HAL_METER_H
#define APP_HAL_METER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_HAL_METER_TEMPERATURE, APP_HAL_METER_LIGHT, APP_HAL_METER_DBFS,
    APP_HAL_METER_SPL, APP_HAL_METER_FREQUENCY, APP_HAL_METER_PAGES
} app_hal_meter_page_t;
typedef enum { APP_HAL_METER_LIVE, APP_HAL_METER_MIN, APP_HAL_METER_MAX } app_hal_meter_view_t;
typedef struct {
    float current, minimum, maximum; /* 全部为该页原始单位：℃、lx、dBFS、dB SPL 或 Hz。 */
    uint8_t has_current, has_extrema, held;
} app_hal_meter_sample_t;

/* 注册显示页和按键。选择页面后才申请该传感器，SPL 需用已知声源校准。 */
app_status_t app_hal_meter_init(void);
app_status_t app_hal_meter_de_init(void);
/* 立即切换前台。K1 下一页，K2 保持/恢复，K3 切换实时/最小/最大，K4 清除本页极值。
 * 温度红、光照黄、dBFS 蓝、SPL 品红、Hz 青。光照/频率超过 9999 后显示 klx（绿）/kHz（白）。
 * 恒定声音的负无穷 dBFS 和暂无有效测量均显示 ----。 */
app_status_t app_hal_meter_select_page(app_hal_meter_page_t page);
/* 在参考声场中校准 SPL 页，校准期间的麦克风引用保持到 meter_de_init。 */
app_status_t app_hal_meter_calibrate_spl(float reference_db_spl);
app_status_t app_hal_meter_hold(uint8_t hold);
app_status_t app_hal_meter_show_peak(app_hal_meter_view_t view);
app_status_t app_hal_meter_reset_peaks(void);
app_status_t app_hal_meter_get(app_hal_meter_page_t page, app_hal_meter_sample_t *sample);
/* 统一调度器调用。保持仅锁定实时显示，当前值和本页极值继续采集。 */
app_status_t app_hal_meter_update(void);

#ifdef __cplusplus
}
#endif
#endif
