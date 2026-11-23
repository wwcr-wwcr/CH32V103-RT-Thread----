/******************************************************************************
 * 文件名   : wave_ana.c
 * 说明     : 示波器分析实现 —— 时域统计 + FFT 找主频 + 判波形类型
 *
 * 流程：
 *     1. 遍历一遍：最高值、最低值、平均值（= 直流偏置）
 *     2. 调 HAL/fft 算幅值谱（内部自带去直流）
 *     3. 在 WAVE_ANA_BAND_LO~HI 对应的 bin 区间里找最大的那根 -> 主频 + 幅值
 *     4. 判波形类型（只分正弦 / 方波 / 三角）
 *
 * 判形状的两套办法：
 *   ① 谐波比（准，前提是能看见 3 次谐波，即基频 <= 采样率/6）
 *         方波 h3 = 1/3 基波
 *         三角 h3 = 1/9 基波
 *         正弦 没有谐波
 *   ② 波峰因数（兜底，基频高时谐波超出奈奎斯特，用这个）
 *         CF = 峰值 / 有效值：方波 1000、正弦 1414、三角 1732（千分比）
 ******************************************************************************/

#include "wave_ana.h"

/* 幅值谱缓冲：count 上限 1024 -> 需要 513 个点（1026 字节） */
#define WAVE_ANA_BINS   ((uint16_t)(FFT_COUNT_MAX / 2U + 1U))

/* 低于这个幅值就认为没信号（码值，满量程 4095） */
#define WAVE_ANA_MIN_AMP   8U

static uint16_t s_amp[WAVE_ANA_BINS];

/**************************************************************************************************
 *                                        内部工具
 **************************************************************************************************/
static uint32_t isqrt32(uint32_t v)
{
    uint32_t r = 0;
    uint32_t bit = 1UL << 30;

    while(bit > v)
    {
        bit >>= 2;
    }

    while(bit != 0)
    {
        if(v >= r + bit)
        {
            v -= r + bit;
            r = (r >> 1) + bit;
        }
        else
        {
            r >>= 1;
        }
        bit >>= 2;
    }

    return r;
}

/* 取 k 附近的最大值（非整数 bin 时能量会漏到隔壁一格） */
static uint16_t peak_near(const uint16_t *amp, uint16_t k, uint16_t bins)
{
    uint16_t m;

    if((k == 0U) || (k > bins))
    {
        return 0U;
    }

    m = amp[k];

    if((k > 1U) && (amp[k - 1U] > m))
    {
        m = amp[k - 1U];
    }

    if(((uint16_t)(k + 1U) <= bins) && (amp[k + 1U] > m))
    {
        m = amp[k + 1U];
    }

    return m;
}

/**
 * @brief  波峰因数判形状（谐波看不到时用）
 * @note   CF = 峰值 / 有效值：方波 1000、正弦 1414、三角 1732（千分比）
 */
static uint8_t shape_by_cf(const uint16_t *s, uint16_t cnt, uint16_t dc, uint16_t vpp)
{
    uint64_t sum2 = 0;
    uint32_t ms;
    uint32_t rms;
    uint32_t cf;
    uint16_t i;

    for(i = 0; i < cnt; i++)
    {
        int32_t d = (int32_t)s[i] - (int32_t)dc;

        sum2 += (uint64_t)(d * d);
    }

    ms  = (uint32_t)(sum2 / cnt);
    rms = isqrt32(ms);

    if(rms == 0U)
    {
        return 0;                       /* 0 = 没信号 */
    }

    cf = ((uint32_t)vpp / 2U) * 1000U / rms;

    if(cf < 1200U)
    {
        return 2;                       /* 2 = 方波：峰值≈有效值 */
    }

    if(cf > 1600U)
    {
        return 3;                       /* 3 = 三角：峰值明显大于有效值 */
    }

    return 1;                           /* 1 = 正弦 */
}

/**************************************************************************************************
 *                                        对外函数
 **************************************************************************************************/
uint32_t wave_ana_mv(uint16_t code)
{
    return (uint32_t)code * 3300U / 4095U;
}

int wave_ana_run(const uint16_t *samples, uint16_t count, uint32_t rate_hz, wave_ana_t *out)
{
    uint32_t sum = 0;
    uint32_t f_hi;
    uint32_t h1;
    uint32_t h3;
    uint16_t lo;
    uint16_t hi;
    uint16_t i;
    uint16_t bins;
    uint16_t k_lo;
    uint16_t k_hi;
    uint16_t peak;
    uint16_t best;
    uint16_t n;

    if((samples == 0) || (out == 0) || (count < FFT_COUNT_MIN) || (count > FFT_COUNT_MAX)
       || (rate_hz == 0U))
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
    out->shape   = 0;                               /* 0 = 没信号 */

    if(n == 0U)
    {
        out->valid = 0;
        return -1;
    }

    bins = (uint16_t)(n / 2U);

    /* 3. 把识别频段换算成 bin 区间（上限不能超过 采样率/2） */
    f_hi = rate_hz / 2U;
    if(f_hi > WAVE_ANA_BAND_HI)
    {
        f_hi = WAVE_ANA_BAND_HI;
    }

    if(f_hi <= WAVE_ANA_BAND_LO)
    {
        out->valid = 0;
        return -2;                      /* 采样率太低，这个频段根本看不到 */
    }

    k_lo = (uint16_t)(((WAVE_ANA_BAND_LO * (uint32_t)n) + rate_hz - 1U) / rate_hz);   /* 向上取整 */
    k_hi = (uint16_t)((f_hi * (uint32_t)n) / rate_hz);                                /* 向下取整 */

    if(k_lo < 1U) k_lo = 1U;            /* bin0 是直流，不要 */
    if(k_hi > bins) k_hi = bins;

    if(k_lo > k_hi)
    {
        out->valid = 0;
        return -2;
    }

    /* 4. 只在这个区间里找最响的频点 */
    peak = 0;
    best = 0;

    for(i = k_lo; i <= k_hi; i++)
    {
        if(s_amp[i] > best)
        {
            best = s_amp[i];
            peak = i;
        }
    }

    if((peak > 0U) && (best > 0U))
    {
        out->freq_hz = (uint32_t)peak * rate_hz / (uint32_t)n;
        out->amp     = best;
    }

    /* 5. 判波形类型 */
    if(best < WAVE_ANA_MIN_AMP)
    {
        out->shape = 0;                                 /* 0 = 没信号 */
    }
    else if(((uint32_t)peak * 3U) > (uint32_t)bins)
    {
        /* 基频太高，3 次谐波超出奈奎斯特 -> 用波峰因数兜底 */
        out->shape = shape_by_cf(samples, count, out->dc, out->vpp);
    }
    else
    {
        h1 = (uint32_t)best;
        h3 = (uint32_t)peak_near(s_amp, (uint16_t)(peak * 3U), bins);

        if((h3 * 5U) >= h1)                         /* h3 >= 0.20 h1 -> 方波 */
        {
            out->shape = 2;                                 /* 2 = 方波 */
        }
        else if((h3 * 12U) >= h1)                   /* h3 >= 0.083 h1 -> 三角 */
        {
            out->shape = 3;                                 /* 3 = 三角 */
        }
        else
        {
            out->shape = 1;                                 /* 1 = 正弦 */
        }
    }

    /* 6. 峰峰值太小（< 8 码 ≈ 6 mV）就认为没信号 */
    out->valid = (uint8_t)(((out->vpp >= 8U) && (best >= WAVE_ANA_MIN_AMP)) ? 1 : 0);

    if(out->valid == 0U)
    {
        out->shape = 0;                                 /* 0 = 没信号 */
    }

    return 0;
}

/* ============================================================================
 * 使用示例：整段复制到你的 task 里
 * ========================================================================== */
#if 0
#include "wave.h"
#include "wave_ana.h"

static uint16_t s_buf[512] __attribute__((aligned(2048)));      /* 64 kHz x 512 = 8 ms */

static void ana_tick(void)
{
    wave_ana_t r;

    if(wave_capture(ADC_PB1, s_buf, 512, 64000) != 0)
    {
        return;                         /* 采失败：保持上一帧 */
    }

    if(wave_ana_run(s_buf, 512, 64000, &r) != 0)
    {
        return;
    }

    if(r.valid != 0)
    {
        /* r.shape: 1=正弦 2=方波 3=三角 */
        rt_kprintf("%u Hz shape=%u %u mVpp\n",
                   (unsigned)r.freq_hz,
                   (unsigned)r.shape,
                   (unsigned)wave_ana_mv(r.vpp));
    }
    else
    {
        rt_kprintf("--\n");             /* 没信号或超出频段 */
    }
}
#endif
