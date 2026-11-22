/******************************************************************************
 * 文件名   : fft.c
 * 说明     : 幅值谱（Goertzel 逐频点算法）
 *
 * 为什么不用 radix-2 FFT：
 *     正规矩 FFT 要 re[]/im[] 两个长度 N 的复数数组（N=256 就要 4 KB），
 *     和"驱动里不放缓冲、缓冲由 task 管"的约定冲突。
 *     Goertzel 每个频点只用 3 个 float 中间量，**不占任何数组**，
 *     代价是 O(n^2)：N=256 时约 3.3 万次乘加，144 MHz 下约 1~2 ms，够用。
 *
 * 数值全部用 float 算，但**不依赖 libm**：
 *     余弦  —— 泰勒展开（N>=16 时角度 <= 0.37 rad，6 阶误差 < 1e-7）
 *     开方  —— 整数牛顿迭代（先把幅值平方归一化到 uint32 范围）
 *
 * 频点系数：c(k) = 2*cos(k*theta)，用切比雪夫递推
 *     c(0) = 2,  c(1) = 2*cos(theta),  c(k+1) = c(1)*c(k) - c(k-1)
 ******************************************************************************/

#include "fft.h"

#define FFT_TWO_PI   6.28318530718f

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

/* cos(x)，x 在 0..0.4 rad 之间（调用方保证），泰勒展开到 6 阶 */
static float cos_small(float x)
{
    float x2 = x * x;

    return 1.0f - x2 * 0.5f + x2 * x2 / 24.0f - x2 * x2 * x2 / 720.0f;
}

uint16_t fft_process(const uint16_t *in, uint16_t *out, uint16_t n)
{
    uint32_t sum = 0;
    float    mean;
    float    theta;
    float    c1;            /* 2*cos(theta)，整个循环里不变 */
    float    ck;            /* c(k)   —— 当前频点的系数 */
    float    ckm1;          /* c(k-1) */
    float    coef;          /* 本次 Goertzel 用的系数 */
    float    half;
    uint16_t i;
    uint16_t k;
    uint16_t bins;

    if((in == 0) || (out == 0) || (n < FFT_COUNT_MIN) || (n > FFT_COUNT_MAX))
    {
        return 0;
    }

    /* 1. 平均值（直流偏置），后面每个样点都要减掉 */
    for(i = 0; i < n; i++)
    {
        sum += in[i];
    }
    mean = (float)sum / (float)n;

    /* bin0 = 直流分量本身（绝对值） */
    out[0] = (uint16_t)mean;

    /* 2. 逐频点算幅值 */
    theta = FFT_TWO_PI / (float)n;
    c1    = 2.0f * cos_small(theta);
    ck    = c1;                 /* c(1) */
    ckm1  = 2.0f;               /* c(0) */
    half  = (float)(n / 2U);
    bins  = (uint16_t)(n / 2U);

    for(k = 1; k <= bins; k++)
    {
        float s0 = 0.0f;
        float s1 = 0.0f;
        float s2 = 0.0f;
        float mag2;
        float amp2;
        uint32_t a;

        coef = ck;

        for(i = 0; i < n; i++)
        {
            s0 = ((float)in[i] - mean) + coef * s1 - s2;
            s2 = s1;
            s1 = s0;
        }

        mag2 = s1 * s1 + s2 * s2 - coef * s1 * s2;
        if(mag2 < 0.0f)
        {
            mag2 = 0.0f;
        }

        /* 幅值 = sqrt(mag2) / (n/2)；先归一化再开方，避免溢出 */
        amp2 = mag2 / (half * half);
        a    = isqrt32((uint32_t)(amp2 + 0.5f));

        out[k] = (uint16_t)((a > 4095U) ? 4095U : a);

        /* 下一个频点的系数：c(k+1) = c1*c(k) - c(k-1) */
        {
            float next = c1 * ck - ckm1;

            ckm1 = ck;
            ck   = next;
        }
    }

    return n;
}

/* ============================================================================
 * 使用示例：整段复制到你的 task 里
 * ========================================================================== */
#if 0
#include "fft.h"
#include "wave.h"

static uint16_t s_buf[256] __attribute__((aligned(2048)));   /* 采样缓冲（自己建） */
static uint16_t s_amp[129];                                  /* 256/2 + 1 个幅值 */

static void spectrum_tick(void)
{
    uint16_t k;
    uint16_t peak = 0;
    uint16_t best = 0;

    /* 1. 采一帧（HAL/wave 只负责采样） */
    if(wave_capture(ADC_PA1, s_buf, 256, 8000) < 0)
    {
        return;
    }

    /* 2. 算幅值谱：s_amp[0] = 直流，s_amp[k] = k * (8000/256) Hz 的幅值 */
    FFT_RUN(s_buf, s_amp);

    /* 3. 找主频 */
    for(k = 1; k <= 128; k++)
    {
        if(s_amp[k] > best)
        {
            best = s_amp[k];
            peak = k;
        }
    }

    /* 主频 = peak * 8000 / 256 = peak * 31.25 Hz */
}
#endif
