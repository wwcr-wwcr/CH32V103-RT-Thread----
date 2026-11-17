/******************************************************************************
 * 文件名   : tool_fixed.h
 * 说明     : 定点数与业务换算 —— Q15/Q8 乘除、四舍五入、金额、BMI
 *
 * 赛题里那些"看起来要浮点"的公式，其实都能用整数算干净：
 *   · 任务 7 ETC：重量 × 单价 = 金额（保留到分）
 *   · 任务 7 医疗：BMI = 体重(kg) / 身高(m)^2，还要按中国标准判档
 *   · 任务 4/5：加速度、转速按比例换算，用 Q15 就够
 *
 * 为什么不用 float：CH32V307 有单精度 FPU，但一旦出现 printf("%f") 或
 * double，Flash 立刻多几 KB；比赛板子的 Flash 从来都不宽裕。
 *
 * 用法:
 *     int32_t k = tool_fmul_q15(q15(0.6), q15(0.5));    // 0.3
 *     tool_cents_str(buf, sizeof(buf), 12 * 3450);      // "414.00"
 *     int16_t bmi = tool_bmi_x10(65000, 1700);          // 224 = 22.4
 ******************************************************************************/

#ifndef __TOOL_FIXED_H
#define __TOOL_FIXED_H

#include <stdint.h>

#define TOOL_Q15_ONE        32768L      /* 1.0 对应的 Q15 值 */
#define TOOL_Q8_ONE         256L        /* 1.0 对应的 Q8 值 */

/**
 * @brief  Q15 乘法：a*b/32768，带四舍五入
 * @note   两个操作数都要是 Q15（-32768 ~ 32767 表示 -1 ~ 1）
 */
static inline int32_t tool_fmul_q15(int32_t a, int32_t b)
{
    return (a * b + (TOOL_Q15_ONE / 2)) >> 15;
}

/**
 * @brief  Q15 除法：a/b*32768
 * @return 0 表示 b 为 0
 * @note   a 不能超过 65535（左移 15 位会溢出）
 */
static inline int32_t tool_fdiv_q15(int32_t a, int32_t b)
{
    if(b == 0) { return 0; }

    return (a << 15) / b;
}

/**
 * @brief  Q8 乘法：a*b/256
 */
static inline int32_t tool_fmul_q8(int32_t a, int32_t b)
{
    return (a * b + (TOOL_Q8_ONE / 2)) >> 8;
}

/**
 * @brief  Q8 除法：a/b*256
 */
static inline int32_t tool_fdiv_q8(int32_t a, int32_t b)
{
    if(b == 0) { return 0; }

    return (a << 8) / b;
}

/**
 * @brief  四舍五入的整数除法（负数也正确，不会变成 -0.5 -> 0）
 * @return 0 表示 b 为 0
 * @note   C 的 / 是向零截断：7/2=3、-7/2=-3，
 *         想要 3.5->4、-3.5->-4 就用这个
 */
static inline int32_t tool_round_div(int32_t a, int32_t b)
{
    uint8_t  neg;
    uint32_t ua;
    uint32_t ub;
    uint32_t q;

    if(b == 0) { return 0; }

    neg = (uint8_t)(((a < 0) != (b < 0)) ? 1u : 0u);
    ua  = (a < 0) ? ((uint32_t)(-(a + 1)) + 1u) : (uint32_t)a;
    ub  = (b < 0) ? ((uint32_t)(-(b + 1)) + 1u) : (uint32_t)b;

    q = (ua + (ub / 2u)) / ub;

    return (neg != 0u) ? -(int32_t)q : (int32_t)q;
}

/**
 * @brief  无符号四舍五入除法（算百分比、平均最常用）
 */
static inline uint32_t tool_div_round_u32(uint32_t a, uint32_t b)
{
    if(b == 0u) { return 0u; }

    return (a + (b / 2u)) / b;
}

/**
 * @brief  金额（单位：分）转字符串，固定两位小数
 * @param  cents  -1234 -> "-12.34"
 * @return 写入的字符数
 * @note   重量×单价一般先算成"分"再显示，不然 0.1 元的误差会累积到对不上账
 */
static inline uint8_t tool_cents_str(char *buf, uint8_t cap, int32_t cents)
{
    char     tmp[12];
    uint32_t mag;
    uint8_t  n = 0;
    uint8_t  pos = 0;

    if((buf == 0) || (cap < 8u)) { if((buf != 0) && (cap != 0u)) { buf[0] = '\0'; } return 0; }

    mag = (cents < 0) ? ((uint32_t)(-(cents + 1)) + 1u) : (uint32_t)cents;

    if(cents < 0) { buf[pos++] = '-'; }

    /* 整数部分 */
    {
        uint32_t ip = mag / 100u;

        do
        {
            tmp[n++] = (char)('0' + (uint8_t)(ip % 10u));
            ip /= 10u;
        } while((ip != 0u) && (n < 10u));

        while((n > 0u) && (pos < (uint8_t)(cap - 4u))) { buf[pos++] = tmp[--n]; }
    }

    buf[pos++] = '.';

    /* 两位小数，不足补 0 */
    buf[pos++] = (char)('0' + (uint8_t)((mag / 10u) % 10u));
    buf[pos++] = (char)('0' + (uint8_t)(mag % 10u));

    buf[pos] = '\0';

    return pos;
}

/**
 * @brief  BMI × 10（保留一位小数）
 * @param  weight_g   体重，单位克（65.0kg 传 65000）
 * @param  height_mm  身高，单位毫米（1.70m 传 1700）
 * @return 22.4 -> 224；身高为 0 返回 0
 * @note   BMI = kg / m^2 = (g/1000) / (mm/1000)^2 = g × 10000 / mm^2
 */
static inline int32_t tool_bmi_x10(int32_t weight_g, int32_t height_mm)
{
    uint32_t h2;

    if((weight_g <= 0) || (height_mm <= 0)) { return 0; }

    h2 = (uint32_t)height_mm * (uint32_t)height_mm;

    return (int32_t)(((uint32_t)weight_g * 10000u) / h2);
}

/**
 * @brief  按中国成人标准判 BMI 档位
 * @param  bmi_x10  tool_bmi_x10 的结果
 * @return 0 = 偏瘦(<18.5)  1 = 正常(18.5~23.9)  2 = 超重(24~27.9)  3 = 肥胖(>=28)
 */
static inline uint8_t tool_bmi_class(int32_t bmi_x10)
{
    if(bmi_x10 < 185) { return 0; }
    if(bmi_x10 < 240) { return 1; }
    if(bmi_x10 < 280) { return 2; }

    return 3;
}

#endif
