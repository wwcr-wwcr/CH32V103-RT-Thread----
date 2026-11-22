/******************************************************************************
 * 文件名   : siggen.h
 * 说明     : DAC 输出引擎（PA5 / DAC2）—— 只管"把一张表按某个速率循环打出去"
 *
 * 分工：
 *     本文件只做输出，不生成波形、不含任何波形表/缓冲。
 *     波形由上层（APP/wave_gen）算好写进调用者的表，再交给这里播放。
 *
 * 表的内存由调用者提供并常驻（dac_play 用 DMA 循环读，停止前必须有效）。
 ******************************************************************************/

#ifndef __HAL_SIGGEN_H
#define __HAL_SIGGEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** dac_play 的采样率上限 */
#define SIGGEN_RATE_MAX  1000000U

/** 一张表最少点数（少了波形会走样） */
#define SIGGEN_POINTS_MIN  8U

/**
 * @brief  算播放一张表需要的采样率
 * @param  freq_hz   想要输出的基频（Hz）
 * @param  count     一张表的点数（= 一个周期的点数）
 * @return 采样率；算出来超过 1 MHz 或参数非法时返回 0
 * @note   输出频率 = 采样率 / 点数，所以采样率 = 频率 × 点数
 */
uint32_t siggen_rate(uint32_t freq_hz, uint16_t count);

/**
 * @brief  开始循环输出
 * @param  table            波形表（常驻 SRAM，半字对齐）
 * @param  count            点数 8..65535
 * @param  sample_rate_hz   每秒播放多少点 1..1000000
 * @return 0 = 成功，负 = 参数错或驱动报错
 */
int siggen_play(const uint16_t *table, uint16_t count, uint32_t sample_rate_hz);

/**
 * @brief  停止输出，PA5 归零
 */
int siggen_stop(void);

/**
 * @brief  输出固定电压（不循环），调试用
 * @param  code  0..4095（0 = 0V，4095 ≈ 3.3V）
 */
int siggen_level(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif
