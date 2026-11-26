/* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 */
#ifndef BOARD_NTC_H
#define BOARD_NTC_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 将 PB0（ADC1 通道 8）配置为模拟输入。请在 RT-Thread 线程中调用测温函数。 */
void ntc_init(void);
/* 返回摄氏温度的 10 倍，例如 253 表示 25.3 ℃。采样失败或读数达到量程端点时返回 INT32_MIN。 */
int32_t ntc_get_temperature(void);
/* 返回整数摄氏温度，小数部分向零截断；读取失败时返回 INT32_MIN。 */
int32_t ntc_read_celsius(void);
/* 返回摄氏温度，保留一位小数；读取失败时返回 NAN。 */
float ntc_read_celsius_f(void);

#ifdef __cplusplus
}
#endif
#endif
