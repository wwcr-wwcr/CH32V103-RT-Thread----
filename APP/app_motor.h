#ifndef APP_MOTOR_H
#define APP_MOTOR_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 P11 电机驱动。使用 TIM1_CH1/CH4，INA/INB 初始均为低电平。 */
app_status_t app_motor_init(void);
/* 设定电机方向和 PWM 占空比。percent 为 -100~100，正值驱动 INB，负值驱动 INA，0 停止。
 * 在线程中调用；换向时先关断原方向并等待至少一个完整 PWM 周期，再驱动另一路。 */
app_status_t app_motor_set_speed(int16_t percent);
/* 停止输出并释放两路电机通道，同一时基上的其他通道继续运行。在线程中调用。 */
app_status_t app_motor_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
