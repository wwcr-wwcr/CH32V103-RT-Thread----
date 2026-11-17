/******************************************************************************
 * 文件名   : tool_wave.h
 * 说明     : 波形与脉冲 —— 整数正弦表、相位累加、PWM 占空比、步进间隔
 *
 * 对应赛题里的这几类活：
 *   · 任务 6「正弦波 / PWM 生成」：DAC 或定时器要按频率、幅度、占空比出波形，
 *     还要在屏上把波形画出来（至少 2 个周期）；
 *   · 任务 4「转速表指针」：指针角度要算 sin/cos 求端点；
 *   · 步进电机：转速 rpm 换算成脉冲间隔 us。
 *
 * 全程整数，不调 sinf/浮点库。相位用 32 位累加器表示 0~2π，
 * 频率改的时候只改步进量，波形是连续的（和 HAL/AUDIO 里那套一个思路）。
 *
 * 用法:
 *     uint32_t ph = 0;
 *     uint32_t step = tool_freq_step(1000, 8000);        // 8kHz 采样出 1kHz
 *     tool_wave_fill_sin(buf, 256, step, &ph, 2048, 1500);
 *
 *     int16_t s = tool_sin_q15(30);                      // 16383 (0.5 * 32767)
 *     uint16_t x = cx + (tool_cos_q15(deg) * r >> 15);   // 指针端点
 ******************************************************************************/

#ifndef __TOOL_WAVE_H
#define __TOOL_WAVE_H

#include <stdint.h>

/* 2π 对应的相位满量程（32 位无符号，溢出即回绕，天然周期） */
#define TOOL_PHASE_FULL     0xFFFFFFFFUL

/*********************************************************************
 * Q15 正弦表：256 点 + 末位补 sin(2π)=0 的哨兵，方便线性插值取 i+1
 * 表长 257 × 2B = 514B，放 Flash（const）
 *********************************************************************/
static const int16_t tool_sin_tab[257] =
{
         0,    804,   1608,   2410,   3212,   4011,   4808,   5602,
      6393,   7179,   7962,   8739,   9512,  10278,  11039,  11793,
     12539,  13279,  14010,  14732,  15446,  16151,  16846,  17530,
     18204,  18868,  19519,  20159,  20787,  21403,  22005,  22594,
     23170,  23731,  24279,  24811,  25329,  25832,  26319,  26790,
     27245,  27683,  28105,  28510,  28898,  29268,  29621,  29956,
     30273,  30571,  30852,  31113,  31356,  31580,  31785,  31971,
     32137,  32285,  32412,  32521,  32609,  32678,  32728,  32757,
     32767,  32757,  32728,  32678,  32609,  32521,  32412,  32285,
     32137,  31971,  31785,  31580,  31356,  31113,  30852,  30571,
     30273,  29956,  29621,  29268,  28898,  28510,  28105,  27683,
     27245,  26790,  26319,  25832,  25329,  24811,  24279,  23731,
     23170,  22594,  22005,  21403,  20787,  20159,  19519,  18868,
     18204,  17530,  16846,  16151,  15446,  14732,  14010,  13279,
     12539,  11793,  11039,  10278,   9512,   8739,   7962,   7179,
      6393,   5602,   4808,   4011,   3212,   2410,   1608,    804,
         0,   -804,  -1608,  -2410,  -3212,  -4011,  -4808,  -5602,
     -6393,  -7179,  -7962,  -8739,  -9512, -10278, -11039, -11793,
    -12539, -13279, -14010, -14732, -15446, -16151, -16846, -17530,
    -18204, -18868, -19519, -20159, -20787, -21403, -22005, -22594,
    -23170, -23731, -24279, -24811, -25329, -25832, -26319, -26790,
    -27245, -27683, -28105, -28510, -28898, -29268, -29621, -29956,
    -30273, -30571, -30852, -31113, -31356, -31580, -31785, -31971,
    -32137, -32285, -32412, -32521, -32609, -32678, -32728, -32757,
    -32767, -32757, -32728, -32678, -32609, -32521, -32412, -32285,
    -32137, -31971, -31785, -31580, -31356, -31113, -30852, -30571,
    -30273, -29956, -29621, -29268, -28898, -28510, -28105, -27683,
    -27245, -26790, -26319, -25832, -25329, -24811, -24279, -23731,
    -23170, -22594, -22005, -21403, -20787, -20159, -19519, -18868,
    -18204, -17530, -16846, -16151, -15446, -14732, -14010, -13279,
    -12539, -11793, -11039, -10278,  -9512,  -8739,  -7962,  -7179,
     -6393,  -5602,  -4808,  -4011,  -3212,  -2410,  -1608,   -804,
         0,
};

/**
 * @brief  相位域取正弦（Q15，带线性插值）
 * @param  phase  0 = 0 弧度，TOOL_PHASE_FULL/4 = π/2，满量程回绕
 * @return -32767 ~ 32767
 * @note   每加一次 step 就前进一个采样点，跨周期不用取模
 */
static inline int16_t tool_sin_phase(uint32_t phase)
{
    uint32_t idx  = (phase >> 24) & 0xFFu;          /* 高 8 位选表项 */
    uint32_t frac = (phase >> 10) & 0x3FFFu;        /* 中 14 位做插值 */
    int32_t  a    = tool_sin_tab[idx];
    int32_t  b    = tool_sin_tab[idx + 1u];

    return (int16_t)(a + (((b - a) * (int32_t)frac) >> 14));
}

/**
 * @brief  按角度取正弦（0~359 度，Q15）
 * @note   指针表盘、极坐标画图用它最直观
 */
static inline int16_t tool_sin_q15(int32_t deg)
{
    deg %= 360;
    if(deg < 0) { deg += 360; }

    return tool_sin_phase((uint32_t)deg * 11930464UL);
}

/**
 * @brief  按角度取余弦（Q15），就是 sin(deg + 90)
 */
static inline int16_t tool_cos_q15(int32_t deg)
{
    return tool_sin_q15(deg + 90);
}

/**
 * @brief  频率 -> 每个采样点的相位步进
 * @param  freq_hz    想要的波形频率
 * @param  sample_hz  采样率（DAC 更新率 / 定时器频率）
 * @return 相位步进，喂给 tool_sin_phase 的累加器
 * @note   步进 = freq / sample × 2^32。用了 64 位中间量，
 *         没调用到本函数时链接器不会把 __udivdi3 拖进来
 */
static inline uint32_t tool_freq_step(uint32_t freq_hz, uint32_t sample_hz)
{
    if((sample_hz == 0u) || (freq_hz >= sample_hz)) { return 0; }

    return (uint32_t)(((uint64_t)freq_hz << 32) / sample_hz);
}

/**
 * @brief  往缓冲里填一段正弦（DAC 码值，中点 = 静音/零电平）
 * @param  buf    目标缓冲
 * @param  n      点数
 * @param  step   相位步进（tool_freq_step）
 * @param  phase  相位累加器，跨调用连续；首帧传 0
 * @param  mid    中点码值，DAC 12 位一般 2048
 * @param  amp    峰值幅度（码值），别超过 mid 到上下限的距离
 * @note   返回时 *phase 已经是下一个点的相位，可以接着填下一块
 */
static inline void tool_wave_fill_sin(uint16_t *buf, uint16_t n, uint32_t step,
                                      uint32_t *phase, uint16_t mid, uint16_t amp)
{
    uint16_t i;
    uint32_t ph = (phase != 0) ? *phase : 0u;

    if((buf == 0) || (n == 0u)) { return; }

    for(i = 0; i < n; i++)
    {
        int32_t v = tool_sin_phase(ph);

        ph += step;

        buf[i] = (uint16_t)((int32_t)mid + ((v * (int32_t)amp) >> 15));
    }

    if(phase != 0) { *phase = ph; }
}

/**
 * @brief  往缓冲里填一个完整周期的 PWM 方波（用来在屏上画波形）
 * @param  n         点数（一个周期的采样点数）
 * @param  duty_pct  占空比 0~100
 * @param  high      高电平码值
 * @param  low       低电平码值
 */
static inline void tool_wave_fill_pwm(uint16_t *buf, uint16_t n, uint8_t duty_pct,
                                      uint16_t high, uint16_t low)
{
    uint16_t i;
    uint16_t on;

    if((buf == 0) || (n == 0u)) { return; }
    if(duty_pct > 100u) { duty_pct = 100u; }

    on = (uint16_t)(((uint32_t)n * duty_pct) / 100u);

    for(i = 0; i < n; i++)
    {
        buf[i] = (i < on) ? high : low;
    }
}

/**
 * @brief  定时器 PWM：按占空比算比较值
 * @param  arr_ticks  自动重装值（ARR），周期 = ARR + 1 个计数
 * @param  duty_pct   占空比 0~100
 * @return 写进 CCR 的值
 * @note   和 TIM_SetCompareX(TIMx, tool_duty_ticks(arr, 30)) 配套
 */
static inline uint32_t tool_duty_ticks(uint32_t arr_ticks, uint8_t duty_pct)
{
    if(duty_pct > 100u) { duty_pct = 100u; }

    return (((arr_ticks + 1u) * duty_pct) / 100u);
}

/**
 * @brief  步进电机：转速 -> 每步的间隔时间
 * @param  rpm            转速（转/分）
 * @param  steps_per_rev  一圈多少个脉冲（整步 200，8 细分 1600）
 * @return 每步多少微秒；rpm 为 0 时返回 0（表示停）
 * @note   60000000us / (rpm × steps)，4000rpm / 200 步 = 75us 一步
 */
static inline uint32_t tool_rpm_interval_us(uint32_t rpm, uint16_t steps_per_rev)
{
    if((rpm == 0u) || (steps_per_rev == 0u)) { return 0; }

    return 60000000UL / (rpm * (uint32_t)steps_per_rev);
}

#endif
