/******************************************************************************
 * 文件名   : tool_math.h
 * 说明     : 常用整数数学 —— 夹取 / 线性映射 / 开方 / 取对数 / 伪随机
 *
 * 只用 <stdint.h>，不依赖 RT-Thread、不依赖 libm，随便往哪个目录里丢都能编。
 *
 * 用法:
 *     #include "tool/tool.h"
 *
 *     int32_t p = tool_map(adc, 0, 4095, 0, 100);     // 码值映射成百分比
 *     uint32_t r = tool_isqrt(v * v);                 // 整数开方，不拖 libm
 *
 * 为什么不用浮点：
 *     CH32V307 有 FPU（单精度），但 printf("%f") 和 libm 的 sin/sqrt 一套下来
 *     要吃掉十几 KB Flash。UI 里只是画个曲线、算个百分比，整型完全够。
 ******************************************************************************/

#ifndef __TOOL_MATH_H
#define __TOOL_MATH_H

#include <stdint.h>

/*********************************************************************
 * 宏：单次求值的场景用它最省（注意 a/b 会被求值多次，别传 a++）
 *********************************************************************/
#define TOOL_MIN(a, b)          (((a) < (b)) ? (a) : (b))
#define TOOL_MAX(a, b)          (((a) > (b)) ? (a) : (b))
#define TOOL_ABS(x)             (((x) < 0) ? -(x) : (x))
#define TOOL_SWAP(a, b)         do { __typeof__(a) _t = (a); (a) = (b); (b) = _t; } while(0)

/**
 * @brief  把 v 夹到 [lo, hi]
 * @note   传反了（lo > hi）会自动交换，不会算出一堆负数
 */
static inline int32_t tool_clamp(int32_t v, int32_t lo, int32_t hi)
{
    int32_t t;

    if(lo > hi)
    {
        t  = lo;
        lo = hi;
        hi = t;
    }

    if(v < lo) { return lo; }
    if(v > hi) { return hi; }

    return v;
}

/**
 * @brief  线性映射：把 [in_lo, in_hi] 的 v 映射到 [out_lo, out_hi]，超范围夹住
 * @note   纯整数运算，正向反向都支持（in_lo > in_hi 也行）
 *         约束：(输入跨度) × (输出跨度) 不能超过 2^31，
 *         即 4095 × 1000 这种量级没问题，4096 × 1000000 会溢出
 */
static inline int32_t tool_map(int32_t v, int32_t in_lo, int32_t in_hi,
                               int32_t out_lo, int32_t out_hi)
{
    int32_t span = in_hi - in_lo;

    if(span == 0) { return out_lo; }

    v = tool_clamp(v, TOOL_MIN(in_lo, in_hi), TOOL_MAX(in_lo, in_hi));

    return out_lo + ((v - in_lo) * (out_hi - out_lo)) / span;
}

/**
 * @brief  在两个值之间按百分比插值
 * @param  pct  0 = 全 a，100 = 全 b（超过 100 会被夹住）
 */
static inline int32_t tool_lerp(int32_t a, int32_t b, uint8_t pct)
{
    if(pct > 100u) { pct = 100u; }

    return a + ((b - a) * (int32_t)pct) / 100;
}

/**
 * @brief  整数开方（向下取整），不依赖 math.h
 * @param  v  任意 32 位无符号数
 * @return floor(sqrt(v))，例如 1000 -> 31，1024 -> 32
 * @note   经典移位相减法，约 16 次循环，比浮点 sqrt 省得多
 */
static inline uint32_t tool_isqrt(uint32_t v)
{
    uint32_t root = 0;
    uint32_t bit  = 1UL << 30;

    while(bit > v) { bit >>= 2; }

    while(bit != 0u)
    {
        if(v >= root + bit)
        {
            v    -= root + bit;
            root  = (root >> 1) + bit;
        }
        else
        {
            root >>= 1;
        }

        bit >>= 2;
    }

    return root;
}

/**
 * @brief  向下取整的 log2：2 的整数次幂用得上（FFT 点数、缓冲区大小）
 * @return 0 -> 0，1 -> 0，2 -> 1，1023 -> 9，1024 -> 10
 */
static inline uint8_t tool_log2u(uint32_t v)
{
    uint8_t n = 0;

    while(v > 1u)
    {
        v >>= 1;
        n++;
    }

    return n;
}

/**
 * @brief  向上取到最近的 2 的幂（DMA 缓冲长度、环形缓冲必须 2 的幂时用）
 * @return 0/1 -> 1，3 -> 4，5 -> 8，512 -> 512
 */
static inline uint32_t tool_pow2_ceil(uint32_t v)
{
    uint32_t p = 1;

    while(p < v) { p <<= 1; }

    return p;
}

/**
 * @brief  环形回绕：把 v 折到 [lo, hi] 里，越界就从另一头接着数
 * @note   数码管倒数 9999 -> 0 -> 9999、菜单光标 0..N-1 循环都用它
 */
static inline int32_t tool_wrap(int32_t v, int32_t lo, int32_t hi)
{
    int32_t span;

    if(hi <= lo) { return lo; }

    span = hi - lo + 1;
    v    = (v - lo) % span;
    if(v < 0) { v += span; }

    return v + lo;
}

/**
 * @brief  线性同余伪随机数（不用 srand，种子自己存）
 * @param  seed  种子指针，首次给个非 0 值，之后每调一次自动更新
 * @return 0~65535 的随机数
 * @note   要 0~99 就 tool_rand(&s) % 100；动画抖动、彩灯流水够用，别拿去做密码
 */
static inline uint32_t tool_rand(uint32_t *seed)
{
    *seed = (*seed * 1664525UL) + 1013904223UL;

    return *seed >> 16;
}

/**
 * @brief  百分比：part / total -> 0~100（自动夹住，除零返回 0）
 * @note   part 不要超过 4200 万，否则 part*100 溢出（ADC / 计数场景远达不到）
 */
static inline uint8_t tool_pct_u32(uint32_t part, uint32_t total)
{
    if(total == 0u)   { return 0;   }
    if(part >= total) { return 100; }

    return (uint8_t)((part * 100u) / total);
}

/**
 * @brief  两数之差的绝对值（无符号，不会因为 a<b 变成负数）
 */
static inline uint32_t tool_diff(uint32_t a, uint32_t b)
{
    return (a > b) ? (a - b) : (b - a);
}

#endif
