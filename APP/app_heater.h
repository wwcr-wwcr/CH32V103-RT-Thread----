#ifndef APP_HEATER_H
#define APP_HEATER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化加热片并保持关闭。无参数。PA3 使用独立 TIM9，重复初始化返回 BUSY。 */
app_status_t app_heater_init(void);
/* 设置加热片的 PWM 加热占空比。percent 为 0~100，0 表示关闭，100 表示全开。
 * 仅在初始化后调用。百分比表示额定功率的占比，应用可依据 NTC 温度调整功率。 */
app_status_t app_heater_set_power(uint8_t percent);
/* 关闭加热片并释放 PA3 / TIM9_CH2，同一时基上的其他通道继续运行。 */
app_status_t app_heater_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
