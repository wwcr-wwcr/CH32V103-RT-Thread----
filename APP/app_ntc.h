#ifndef APP_NTC_H
#define APP_NTC_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 板图 R36、R37 均为 10 kΩ，R37 标称温度 25 ℃。B 值取套件参考 HAL，
 * 原理图未给 B 值；如换料，可在工程宏中按元件规格覆盖。 */
#ifndef APP_NTC_BETA_K
#define APP_NTC_BETA_K 3380.0f
#endif
typedef struct {
    uint16_t adc;
    float resistance_ohm;
    float temperature_c;
} app_ntc_sample_t;

/* 初始化板载 NTC 的 ADC1/PB0；与麦克风同时使用。 */
app_status_t app_ntc_init(void);
/* 采 16 点取平均，一次得到原码、电阻和温度。sample 仅在成功时更新；
 * 采样到地或满量程时返回 APP_ERR_IO，可另用 read_raw 检查线路。 */
app_status_t app_ntc_read(app_ntc_sample_t *sample);
/* 读取 16 点平均原码，0~4095。 */
app_status_t app_ntc_read_raw(uint16_t *adc);
/* 直接读取温度（℃）；使用同样的平均和断线检查。 */
app_status_t app_ntc_read_celsius(float *temperature_c);
/* 释放 NTC 使用的 ADC1 和 PB0。 */
app_status_t app_ntc_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
