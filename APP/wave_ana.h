/******************************************************************************
 * 文件名   : wave_ana.h
 * 说明     : 示波器分析 —— 给一段 ADC 采样，返回频率/幅值/直流/最高最低/波形类型
 *
 * 一次调用返回：
 *     频率      freq_hz  主频（只在下面两个宏划定的频段里找）
 *     幅值      amp      主频那根柱子的幅值（码值，和时域幅度同量纲）
 *     直流偏置  dc       整段采样的平均值
 *     最高值    vmax     单点最大码值
 *     最低值    vmin     单点最小码值
 *     峰峰值    vpp      附带给你，等于 vmax - vmin
 *     波形类型  shape    1=正弦  2=方波  3=三角  0=没信号/判不出来
 *
 * 全部是"码值"，要毫伏就调 wave_ana_mv(code) 现算，结构体里不存毫伏。
 *
 * 用法：
 *     uint16_t buf[512] __attribute__((aligned(2048)));
 *     wave_ana_t r;
 *
 *     wave_capture(ADC_PB1, buf, 512, 64000);       // HAL/wave 只采样
 *     wave_ana_run(buf, 512, 64000, &r);            // 一次性算完
 *
 * 内存：本模块内部有一个 1026 字节的幅值缓冲（count<=1024），不用你管。
 ******************************************************************************/

#ifndef __WAVE_ANA_H
#define __WAVE_ANA_H

#include <stdint.h>
#include "fft.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- 识别频段：要改就改这两个数 ---- */
#define WAVE_ANA_BAND_LO    1000U       /* 下限 1 kHz */
#define WAVE_ANA_BAND_HI    30000U      /* 上限 30 kHz（采样率要 >= 60 kHz 才看得到） */

typedef struct
{
    uint16_t vmax;        /* 最高值（码值） */
    uint16_t vmin;        /* 最低值（码值） */
    uint16_t vpp;         /* 峰峰值（码值） */
    uint16_t dc;          /* 直流偏置（码值） */
    uint16_t amp;         /* 主频幅值（码值） */
    uint32_t freq_hz;     /* 主频频率（Hz），分辨率 = 采样率 / 点数 */

    uint8_t  valid;       /* 1 = 信号够大，freq/amp/shape 可信 */
    uint8_t  shape;       /* 1=正弦  2=方波  3=三角  0=没信号 */
} wave_ana_t;

/**
 * @brief  分析一段采样（频段由 WAVE_ANA_BAND_LO/HI 决定）
 * @param  samples  采样数组（12 位码值），长度 count
 * @param  count    点数 16..1024
 * @param  rate_hz  采样率（Hz），用来把频点号换算成频率
 * @param  out      结果写到这里
 * @return 0 = 成功；-1 = 参数错；-2 = 采样率看不到这个频段（比如 8 kHz 采样要看 30 kHz）
 */
int wave_ana_run(const uint16_t *samples, uint16_t count, uint32_t rate_hz, wave_ana_t *out);

/**
 * @brief  码值转毫伏（3.3V 参考，12 位）
 * @note   界面上要显示就现算：snprintf(buf, sizeof(buf), "%u mV", wave_ana_mv(r.amp));
 */
uint32_t wave_ana_mv(uint16_t code);

#ifdef __cplusplus
}
#endif

#endif
