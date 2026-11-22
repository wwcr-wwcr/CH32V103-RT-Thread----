/******************************************************************************
 * 文件名   : wave.h
 * 说明     : ADC 采样（只管采样，不做任何分析）
 *
 * 为什么只剩采样：
 *     采样是"拿到数据"，分析是"看懂数据"。分析放到了 APP/wave_ana.c，
 *     这样这里永远不碰大数组、不占 RAM，缓冲由调用者（task）自己建。
 *
 * 典型用法：
 *     static uint16_t buf[256] __attribute__((aligned(2048)));
 *     wave_capture(ADC_PB1, buf, 256, 5000);      // 采一帧
 *     // 然后交给 APP/wave_ana 或 HAL/fft 去分析
 ******************************************************************************/

#ifndef __HAL_WAVE_H
#define __HAL_WAVE_H

#include <stdint.h>
#include "adc.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  采一帧到调用者的缓冲里
 * @param  input     ADC_PA1（麦克风）或 ADC_PB1（板上模拟输入）
 * @param  buf       缓冲，由 task 提供；2 字节对齐、单帧不跨 64 KiB
 * @param  count     点数 1..65535
 * @param  rate_hz   定时采样率；0 = 全速（用 adc_max_rate_hz() 查询）
 * @return 0 = 成功，负 = 驱动返回的错误码
 */
int wave_capture(adc_input_t input, uint16_t *buf, uint16_t count, uint32_t rate_hz);

#ifdef __cplusplus
}
#endif

#endif
