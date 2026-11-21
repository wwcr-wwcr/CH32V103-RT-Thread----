#ifndef APP_FAN_H
#define APP_FAN_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化风扇并保持关闭。无参数。PA0 使用独立 TIM5，重复初始化返回 BUSY。 */
app_status_t app_fan_init(void);
/* 设置风扇的 PWM 驱动占空比。percent 为 0~100，0 表示关闭，100 表示全开。
 * 仅在初始化后调用。百分比表示驱动量，实际转速取决于风扇和负载。 */
app_status_t app_fan_set_speed(uint8_t percent);
/* 关闭风扇并释放 PA0 / TIM5_CH1，同一时基上的其他通道继续运行。 */
app_status_t app_fan_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
