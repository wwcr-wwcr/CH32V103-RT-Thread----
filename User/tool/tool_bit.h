/******************************************************************************
 * 文件名   : tool_bit.h
 * 说明     : 位操作 —— 置位/清位、位段读写、位反转、统计
 *
 * 寄存器配置、数码管段码、协议标志位都离不开这些。
 * 位段读写（tool_field_get/set）比手写 (x >> 6) & 0x03 更不容易错，
 * 尤其是 VEML7700 的 ALS_CONF 那种"一个寄存器里塞 4 个字段"的地方。
 *
 * 用法:
 *     v = tool_field_get(conf, CONF_IT_SHIFT, 4);        // 取积分时间
 *     conf = tool_field_set(conf, CONF_GAIN_SHIFT, 2, 2); // 改增益
 *     seg = tool_reverse8(0x01);                          // 段码位序反了
 ******************************************************************************/

#ifndef __TOOL_BIT_H
#define __TOOL_BIT_H

#include <stdint.h>

/*********************************************************************
 * 宏
 *********************************************************************/
#define TOOL_BIT(n)             (1UL << (n))
#define TOOL_SET_BIT(v, n)      ((v) |=  TOOL_BIT(n))
#define TOOL_CLR_BIT(v, n)      ((v) &= ~TOOL_BIT(n))
#define TOOL_TOG_BIT(v, n)      ((v) ^=  TOOL_BIT(n))
#define TOOL_GET_BIT(v, n)      (((v) >> (n)) & 1UL)

/**
 * @brief  取位段（读寄存器里的多 bit 字段）
 * @param  v      寄存器值
 * @param  shift  起始位
 * @param  width  位宽
 * @return 右对齐的字段值（已做掩码，高位全是 0）
 */
static inline uint32_t tool_field_get(uint32_t v, uint8_t shift, uint8_t width)
{
    uint32_t mask;

    if(width == 0u) { return 0; }
    if(width >= 32u) { return (shift == 0u) ? v : (v >> shift); }

    mask = (1UL << width) - 1u;

    return (v >> shift) & mask;
}

/**
 * @brief  写位段（先清掉原来的字段，再塞新值，其它位不动）
 * @param  v      原寄存器值
 * @param  shift  起始位
 * @param  width  位宽
 * @param  val    要写入的值（超宽会被掩码截断）
 * @return 改好的寄存器值
 */
static inline uint32_t tool_field_set(uint32_t v, uint8_t shift, uint8_t width, uint32_t val)
{
    uint32_t mask;

    if(width == 0u) { return v; }

    if(width >= 32u)
    {
        return (shift == 0u) ? val : v;         /* 整字写入就不掩了 */
    }

    mask = ((1UL << width) - 1u) << shift;

    return (v & ~mask) | ((val << shift) & mask);
}

/**
 * @brief  数 1 的个数（判断标志位有几个开着、汉明距离）
 */
static inline uint8_t tool_popcount(uint32_t v)
{
    uint8_t n = 0;

    while(v != 0u)
    {
        v &= (v - 1u);          /* 每次干掉最低位的 1 */
        n++;
    }

    return n;
}

/**
 * @brief  8 位位序反转
 * @note   数码管段码 / SPI 低位先出接成高位先出时，这一行能救命
 */
static inline uint8_t tool_reverse8(uint8_t b)
{
    b = (uint8_t)(((b & 0xF0u) >> 4) | ((b & 0x0Fu) << 4));
    b = (uint8_t)(((b & 0xCCu) >> 2) | ((b & 0x33u) << 2));
    b = (uint8_t)(((b & 0xAAu) >> 1) | ((b & 0x55u) << 1));

    return b;
}

/**
 * @brief  奇偶校验位（0 = 偶数个 1，1 = 奇数个 1）
 */
static inline uint8_t tool_parity(uint32_t v)
{
    return (uint8_t)(tool_popcount(v) & 1u);
}

/**
 * @brief  最低那个 1 在第几位
 * @return 位号 0~31；v == 0 返回 0xFF
 * @note   找"第一个有响应的通道"最方便
 */
static inline uint8_t tool_lowest_bit(uint32_t v)
{
    uint8_t n = 0;

    if(v == 0u) { return 0xFFu; }

    while((v & 1u) == 0u)
    {
        v >>= 1;
        n++;
    }

    return n;
}

/**
 * @brief  32 位循环左移（n 会自动对 32 取模，传 33 等于 1）
 */
static inline uint32_t tool_rol32(uint32_t v, uint8_t n)
{
    n = (uint8_t)(n & 31u);

    if(n == 0u) { return v; }

    return (v << n) | (v >> (32u - n));
}

/**
 * @brief  循环右移
 */
static inline uint32_t tool_ror32(uint32_t v, uint8_t n)
{
    n = (uint8_t)(n & 31u);

    if(n == 0u) { return v; }

    return (v >> n) | (v << (32u - n));
}

#endif
