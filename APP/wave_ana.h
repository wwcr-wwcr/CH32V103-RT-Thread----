/******************************************************************************
 * 文件名   : wave_ana.h
 * 说明     : 波形分析 —— 给一段 ADC 采样，直接告诉你这段波形是什么样
 *
 * 一次调用返回 6 个量：
 *     频率      freq_hz  主频（用 FFT 找最强的频点）
 *     幅值      amp      主频那根柱子的幅值（码值，和时域幅度同量纲）
 *     直流偏置  dc       整段采样的平均值
 *     最高值    vmax     单点最大码值
 *     最低值    vmin     单点最小码值
 *     峰峰值    vpp      附带给你，等于 vmax - vmin
 *   另外附一套毫伏值（*_mv），界面直接显示不用再换算。
 *
 * 用法：
 *     uint16_t buf[256];
 *     wave_ana_t r;
 *     wave_capture(ADC_PB1, buf, 256, 5000);        // HAL/wave 只采样
 *     wave_ana_run(buf, 256, 5000, &r);             // 这里一次性算完
 *     // r.freq_hz / r.amp_mv / r.dc_mv / r.vmax_mv / r.vmin_mv
 *
 * 内存：本模块内部有一个 514 字节的幅值缓冲（count<=512），不用你管。
 ******************************************************************************/

#ifndef __WAVE_ANA_H
#define __WAVE_ANA_H

#include <stdint.h>
#include "fft.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t vmax;        /* 最高值（码值） */
    uint16_t vmin;        /* 最低值（码值） */
    uint16_t vpp;         /* 峰峰值（码值） */
    uint16_t dc;          /* 直流偏置（码值） */
    uint16_t amp;         /* 主频幅值（码值） */
    uint32_t freq_hz;     /* 主频频率（Hz），分辨率 = 采样率 / 点数 */

    uint16_t vmax_mv;     /* 下面四个是上面几个的毫伏版（3.3V 参考） */
    uint16_t vmin_mv;
    uint16_t dc_mv;
    uint16_t amp_mv;

    uint8_t  valid;       /* 1 = 信号够大，freq/amp 可信 */
} wave_ana_t;

/**
 * @brief  分析一段采样
 * @param  samples  采样数组（12 位码值），长度 count
 * @param  count    点数 16..512
 * @param  rate_hz  采样率（Hz），用来把频点号换算成频率
 * @param  out      结果写到这里
 * @return 0 = 成功；负 = 参数错（out 不改）
 */
int wave_ana_run(const uint16_t *samples, uint16_t count, uint32_t rate_hz, wave_ana_t *out);

/**
 * @brief  码值转毫伏（3.3V 参考，12 位）
 */
uint32_t wave_ana_mv(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif
