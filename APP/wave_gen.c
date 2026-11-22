/******************************************************************************
 * 文件名   : wave_gen.c
 * 说明     : 波形发生器实现 —— 内置 256 点正弦码表，4 种波形全在这
 *
 * 内存：
 *     s_sine[256]   const，512 字节，放 Flash（码表本身）
 *     s_buf[1024]   static，2048 字节，放 RAM（输出表，DMA 循环读）
 *     两个数组都只在本文件，外部看不到。
 ******************************************************************************/

#include "wave_gen.h"
#include "siggen.h"
#include <rtthread.h>

#define WG_POINTS_MAX   1024U       /* 输出表容量（扫频用满，其它波形用前面一段） */
#define WG_FINE_POINTS  256U        /* 码表精度：一个周期 256 点 */
#define WG_MIN_POINTS   32U         /* 高频率时每周期最少点数 */

/* 256 点正弦码表，归一化到 ±32000（12 位输出时按 low/high 缩放） */
static const int16_t s_sine[WG_FINE_POINTS] =
{
         0,    785,   1570,   2354,   3137,   3917,   4695,   5471,   6243,   7011,   7775,   8535,   9289,  10038,  10780,  11517,
     12246,  12968,  13682,  14388,  15085,  15773,  16451,  17120,  17778,  18426,  19062,  19687,  20301,  20902,  21490,  22065,
     22627,  23176,  23710,  24231,  24736,  25227,  25703,  26163,  26607,  27035,  27447,  27843,  28221,  28583,  28928,  29255,
     29564,  29856,  30129,  30385,  30622,  30841,  31041,  31222,  31385,  31529,  31654,  31759,  31846,  31913,  31961,  31990,
     32000,  31990,  31961,  31913,  31846,  31759,  31654,  31529,  31385,  31222,  31041,  30841,  30622,  30385,  30129,  29856,
     29564,  29255,  28928,  28583,  28221,  27843,  27447,  27035,  26607,  26163,  25703,  25227,  24736,  24231,  23710,  23176,
     22627,  22065,  21490,  20902,  20301,  19687,  19062,  18426,  17778,  17120,  16451,  15773,  15085,  14388,  13682,  12968,
     12246,  11517,  10780,  10038,   9289,   8535,   7775,   7011,   6243,   5471,   4695,   3917,   3137,   2354,   1570,    785,
         0,   -785,  -1570,  -2354,  -3137,  -3917,  -4695,  -5471,  -6243,  -7011,  -7775,  -8535,  -9289, -10038, -10780, -11517,
    -12246, -12968, -13682, -14388, -15085, -15773, -16451, -17120, -17778, -18426, -19062, -19687, -20301, -20902, -21490, -22065,
    -22627, -23176, -23710, -24231, -24736, -25227, -25703, -26163, -26607, -27035, -27447, -27843, -28221, -28583, -28928, -29255,
    -29564, -29856, -30129, -30385, -30622, -30841, -31041, -31222, -31385, -31529, -31654, -31759, -31846, -31913, -31961, -31990,
    -32000, -31990, -31961, -31913, -31846, -31759, -31654, -31529, -31385, -31222, -31041, -30841, -30622, -30385, -30129, -29856,
    -29564, -29255, -28928, -28583, -28221, -27843, -27447, -27035, -26607, -26163, -25703, -25227, -24736, -24231, -23710, -23176,
    -22627, -22065, -21490, -20902, -20301, -19687, -19062, -18426, -17778, -17120, -16451, -15773, -15085, -14388, -13682, -12968,
    -12246, -11517, -10780, -10038,  -9289,  -8535,  -7775,  -7011,  -6243,  -5471,  -4695,  -3917,  -3137,  -2354,  -1570,   -785
};

/* 输出表：DAC 用 DMA 循环读，必须常驻 */
static uint16_t s_buf[WG_POINTS_MAX];

/**************************************************************************************************
 *                                        内部工具
 **************************************************************************************************/
/**
 * @brief  按频率挑每周期点数：保证 频率 × 点数 <= 1 MHz
 */
static uint16_t pick_points(uint32_t freq_hz)
{
    uint16_t n = (uint16_t)WG_FINE_POINTS;

    while((n > WG_MIN_POINTS) && (freq_hz * (uint32_t)n > SIGGEN_RATE_MAX))
    {
        n = (uint16_t)(n >> 1);
    }

    return n;
}

/**
 * @brief  相位（0..65535）查码表，返回 ±32000
 */
static int32_t sine_at(uint16_t phase)
{
    uint32_t idx = ((uint32_t)phase * WG_FINE_POINTS) >> 16;

    if(idx >= WG_FINE_POINTS)
    {
        idx = WG_FINE_POINTS - 1U;
    }

    return (int32_t)s_sine[idx];
}

/**
 * @brief  检查一套输出范围，顺便算出中心与半幅
 */
static int range_of(uint16_t low, uint16_t high, int32_t *mid, int32_t *amp)
{
    if((low >= high) || (high > 4095U))
    {
        return -RT_EINVAL;
    }

    *mid = ((int32_t)low + (int32_t)high) / 2;
    *amp = ((int32_t)high - (int32_t)low) / 2;

    return 0;
}

/**
 * @brief  毫伏 -> 12 位码值（四舍五入，超过 3300mV 直接钳到满量程）
 */
static uint16_t mv_to_code(uint16_t mv)
{
    if(mv > WAVE_GEN_VREF_MV)
    {
        mv = (uint16_t)WAVE_GEN_VREF_MV;
    }

    return (uint16_t)(((uint32_t)mv * 4095U + (WAVE_GEN_VREF_MV / 2U)) / WAVE_GEN_VREF_MV);
}

/**************************************************************************************************
 *                                        1. 正弦
 **************************************************************************************************/
int wave_gen_sine(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv)
{
    uint16_t n;
    uint16_t i;
    uint32_t rate;
    int32_t  mid;
    int32_t  amp;

    if((freq_hz < 1U) || (freq_hz > WAVE_GEN_FREQ_MAX))
    {
        return -RT_EINVAL;
    }

    uint16_t low  = mv_to_code(low_mv);
    uint16_t high = mv_to_code(high_mv);

    if(range_of(low, high, &mid, &amp) != 0)
    {
        return -RT_EINVAL;
    }

    n    = pick_points(freq_hz);
    rate = siggen_rate(freq_hz, n);

    if(rate == 0)
    {
        return -RT_EINVAL;
    }

    for(i = 0; i < n; i++)
    {
        /* 一个周期 n 点 → 相位 = i * 65536 / n */
        uint16_t phase = (uint16_t)(((uint32_t)i << 16) / (uint32_t)n);

        s_buf[i] = (uint16_t)(mid + sine_at(phase) * amp / 32000);
    }

    return siggen_play(s_buf, n, rate);
}

/**************************************************************************************************
 *                                        2. 三角（尖峰可左右偏）
 **************************************************************************************************/
int wave_gen_tri(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv, uint8_t peak_pct)
{
    uint16_t n;
    uint16_t i;
    uint16_t peak;
    uint32_t rate;
    int32_t  mid;
    int32_t  amp;
    int32_t  span;

    if((freq_hz < 1U) || (freq_hz > WAVE_GEN_FREQ_MAX))
    {
        return -RT_EINVAL;
    }

    uint16_t low  = mv_to_code(low_mv);
    uint16_t high = mv_to_code(high_mv);

    if(range_of(low, high, &mid, &amp) != 0)
    {
        return -RT_EINVAL;
    }

    if(peak_pct < 5U)  peak_pct = 5U;
    if(peak_pct > 95U) peak_pct = 95U;

    n    = pick_points(freq_hz);
    rate = siggen_rate(freq_hz, n);

    if(rate == 0)
    {
        return -RT_EINVAL;
    }

    span = (int32_t)high - (int32_t)low;
    peak = (uint16_t)(((uint32_t)n * peak_pct) / 100U);

    if(peak < 1U)         peak = 1U;
    if(peak >= n)         peak = (uint16_t)(n - 1U);

    for(i = 0; i < n; i++)
    {
        if(i < peak)
        {
            /* 上升段：low → high */
            s_buf[i] = (uint16_t)((int32_t)low + span * (int32_t)i / (int32_t)peak);
        }
        else
        {
            /* 下降段：high → low */
            s_buf[i] = (uint16_t)((int32_t)high -
                                  span * (int32_t)(i - peak) / (int32_t)(n - peak));
        }
    }

    return siggen_play(s_buf, n, rate);
}

/**************************************************************************************************
 *                                        3. 方波（占空比可调）
 **************************************************************************************************/
int wave_gen_square(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv, uint8_t duty_pct)
{
    uint16_t n;
    uint16_t i;
    uint16_t on;
    uint32_t rate;
    int32_t  mid;
    int32_t  amp;

    if((freq_hz < 1U) || (freq_hz > WAVE_GEN_FREQ_MAX))
    {
        return -RT_EINVAL;
    }

    uint16_t low  = mv_to_code(low_mv);
    uint16_t high = mv_to_code(high_mv);

    if(range_of(low, high, &mid, &amp) != 0)
    {
        return -RT_EINVAL;
    }

    if(duty_pct < 5U)  duty_pct = 5U;
    if(duty_pct > 95U) duty_pct = 95U;

    n    = pick_points(freq_hz);
    rate = siggen_rate(freq_hz, n);

    if(rate == 0)
    {
        return -RT_EINVAL;
    }

    on = (uint16_t)(((uint32_t)n * duty_pct) / 100U);

    if(on < 1U) on = 1U;
    if(on >= n) on = (uint16_t)(n - 1U);

    for(i = 0; i < n; i++)
    {
        s_buf[i] = (i < on) ? high : low;
    }

    return siggen_play(s_buf, n, rate);
}

/**************************************************************************************************
 *                                        4. 调频（线性扫频）
 **************************************************************************************************/
int wave_gen_sweep(uint32_t f_start, uint32_t f_end, uint32_t sweep_ms,
                   uint16_t low_mv, uint16_t high_mv)
{
    uint32_t n;
    uint32_t fs;
    uint32_t i;
    uint64_t phase = 0;
    int32_t  mid;
    int32_t  amp;

    if((f_start < 1U) || (f_start > WAVE_GEN_FREQ_MAX))
    {
        return -RT_EINVAL;
    }

    if((f_end < 1U) || (f_end > WAVE_GEN_FREQ_MAX))
    {
        return -RT_EINVAL;
    }

    if((sweep_ms < WAVE_GEN_SWEEP_MS_MIN) || (sweep_ms > WAVE_GEN_SWEEP_MS_MAX))
    {
        return -RT_EINVAL;
    }

    uint16_t low  = mv_to_code(low_mv);
    uint16_t high = mv_to_code(high_mv);

    if(range_of(low, high, &mid, &amp) != 0)
    {
        return -RT_EINVAL;
    }

    /* 缓冲区固定 1024 点：采样率 = 点数 / 扫频时长 */
    n  = WG_POINTS_MAX;
    fs = (n * 1000U) / sweep_ms;

    if(fs < 200U)             fs = 200U;    /* 再低就没意义了 */
    if(fs > SIGGEN_RATE_MAX)  fs = SIGGEN_RATE_MAX;

    /* 1024 点缓冲下：fs = 1024 / 时长，结束频率上限 ≈ fs/3
     *   200ms  → fs 5120，最多约 1700 Hz
     *   500ms  → fs 2048，最多约 680 Hz
     *   1000ms → fs 1024，最多约 340 Hz
     * 想要更高频率就缩短扫频时长，两者不能同时要（缓冲就 2KB）。 */

    /* 留 3 倍余量，避免高频端混叠 */
    if(f_end > (fs / 3U)) f_end = fs / 3U;
    if(f_start > f_end)   f_start = f_end;

    for(i = 0; i < n; i++)
    {
        uint32_t f   = f_start + (uint32_t)(((uint64_t)(f_end - f_start) * i) / n);
        uint32_t inc = (uint32_t)(((uint64_t)f << 32) / fs);

        s_buf[i] = (uint16_t)(mid + sine_at((uint16_t)(phase >> 16)) * amp / 32000);

        phase += inc;
    }

    return siggen_play(s_buf, (uint16_t)n, fs);
}

/**************************************************************************************************
 *                                        停止
 **************************************************************************************************/
int wave_gen_stop(void)
{
    return siggen_stop();
}

/* ============================================================================
 * 使用示例：整段复制到 task 里
 * ========================================================================== */
#if 0
#include "wave_gen.h"

static void gen_demo(void)
{
    /* 正弦：1 kHz，输出 0.8V~2.5V */
    wave_gen_sine(1000, 800, 2500);

    /* 三角：500 Hz，0.8V~3.3V，尖峰偏左（30 = 上升快、下降慢） */
    wave_gen_tri(500, 800, 3300, 30);

    /* 方波：2 kHz，0V~3.3V，占空比 20% */
    wave_gen_square(2000, 0, 3300, 20);

    /* 调频：200 Hz → 2 kHz，一次 800 ms，0.8V~3.3V */
    wave_gen_sweep(200, 2000, 800, 800, 3300);

    /* 停止（离开页面前必须调） */
    wave_gen_stop();
}
#endif
