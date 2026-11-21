#ifndef APP_LIGHT_H
#define APP_LIGHT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 启用板载 VEML7700。固定增益 1/8、积分时间 100 ms。 */
app_status_t app_light_init(void);
/* 读取环境光强度（lx）。首次积分期间返回 BUSY；饱和或超过 140 klx 时返回 IO。
 * 成功后可随时读同一结果，传感器约每 100 ms 刷新。 */
app_status_t app_light_read_lux(float *lux);
/* 读取 16 位 ALS 原码，保留满量程值，可用于检查饱和。 */
app_status_t app_light_read_raw(uint16_t *raw);
/* 让光照传感器休眠，释放 I2C 引用。 */
app_status_t app_light_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
