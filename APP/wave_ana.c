/******************************************************************************
 * 文件名   : wave_ana.c
 * 说明     : 波形分析实现 —— 时域统计 + FFT 找主频
 *
 * 流程：
 *     1. 遍历一遍：最高值、最低值、平均值（= 直流偏置）
 *     2. 调 HAL/fft 算幅值谱（内部自带去直流）
 *     3. 在 bin1..count/2 里找最大的那根 → 主频 + 幅值
 ******************************************************************************/

#include "wave_ana.h"

/* 幅值谱缓冲：count 上限 512 → 需要 257 个点（514 字节） */
#define WAVE_ANA_BINS   ((uint16_t)(FFT_COUNT_MAX / 2U + 1U))

static uint16_t s_amp[WAVE_ANA_BINS];

uint32_t wave_ana_mv(uint16_t code)
{
    return (uint32_t)code * 3300U / 4095U;
}

int wave_ana_run(const uint16_t *samples, uint16_t count, uint32_t rate_hz, wave_ana_t *out)
{
    uint32_t sum = 0;
    uint16_t lo;
    uint16_t hi;
    uint16_t i;
    uint16_t bins;
    uint16_t peak;
    uint16_t best;
    uint16_t n;

    if((samples == 0) || (out == 0) || (count < FFT_COUNT_MIN) || (count > FFT_COUNT_MAX))
    {
        return -1;
    }

    /* 1. 时域统计 */
    lo = samples[0];
    hi = samples[0];

    for(i = 0; i < count; i++)
    {
        uint16_t v = samples[i];

        if(v < lo) lo = v;
        if(v > hi) hi = v;
        sum += v;
    }

    out->vmax = hi;
    out->vmin = lo;
    out->vpp  = (uint16_t)(hi - lo);
    out->dc   = (uint16_t)(sum / count);

    /* 2. 幅值谱（HAL/fft 内部会先减掉平均值） */
    n = fft_process(samples, s_amp, count);
    out->freq_hz = 0;
    out->amp     = 0;

    if(n == 0)
    {
        out->valid = 0;
        return -1;
    }

    bins = (uint16_t)(n / 2U);

    /* 3. 找主频：bin0 是直流，从 bin1 开始找 */
    peak = 0;
    best = 0;

    for(i = 1; i <= bins; i++)
    {
        if(s_amp[i] > best)
        {
            best = s_amp[i];
            peak = i;
        }
    }

    if((peak > 0) && (best > 0))
    {
        out->freq_hz = (uint32_t)peak * rate_hz / (uint32_t)n;
        out->amp     = best;
    }

    /* 4. 毫伏版（界面直接用） */
    out->vmax_mv = (uint16_t)wave_ana_mv(out->vmax);
    out->vmin_mv = (uint16_t)wave_ana_mv(out->vmin);
    out->dc_mv   = (uint16_t)wave_ana_mv(out->dc);
    out->amp_mv  = (uint16_t)wave_ana_mv(out->amp);

    /* 峰峰值太小（< 8 码 ≈ 6 mV）就认为没信号，频率不可信 */
    out->valid = (uint8_t)(((out->vpp >= 8) && (best >= 4)) ? 1 : 0);

    return 0;
}

/* ============================================================================
 * 使用示例：整段复制到你的 task 里
 * ========================================================================== */
#if 0
#include "wave.h"
#include "wave_ana.h"

static uint16_t s_buf[256] __attribute__((aligned(2048)));

static void ana_tick(void)
{
    wave_ana_t r;

    if(wave_capture(ADC_PB1, s_buf, 256, 5000) != 0)
    {
        return;                         /* 采失败：保持上一帧 */
    }

    if(wave_ana_run(s_buf, 256, 5000, &r) != 0)
    {
        return;
    }

    if(r.valid != 0)
    {
        /* 例如显示：996 Hz / 1.29 V 峰峰 / 偏置 1.65 V */
    }
    else
    {
        /* 没信号：显示 "--" */
    }
}
#endif
