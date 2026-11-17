/******************************************************************************
 * 文件名   : tool_fmt.h
 * 说明     : 数字转字符串 —— 不依赖 printf/snprintf
 *
 * LVGL 的 label 要的是 char*，每次更新都 snprintf 一遍在小核上有三个毛病：
 *   1) 吃掉 4~6KB Flash（格式化引擎 + 浮点支持）；
 *   2) 栈上临时缓冲 + 可变参数，任务栈要留够；
 *   3) 和 RT-Thread 的 libc 重定向撞上时偶发跑飞。
 * 这里四个函数覆盖了 UI 里 95% 的用法，纯整数运算，几十字节栈。
 *
 * 用法:
 *     char buf[16];
 *     tool_u32_str(buf, sizeof(buf), 42, 10, 3);   // "042"
 *     tool_i32_str(buf, sizeof(buf), -25, 0);      // "-25"
 *     tool_fx_str (buf, sizeof(buf), 253, 1);      // "25.3"
 *     tool_bytes_str(buf, sizeof(buf), p, 4);      // "DE AD BE EF"
 *     ui_text_set(label, buf);
 *
 * 约定：所有函数都保证 buf 以 '\0' 结尾，返回写入的字符数（不含 '\0'）；
 *       缓冲区给不够时按能放下的位数截断，不会越界。
 ******************************************************************************/

#ifndef __TOOL_FMT_H
#define __TOOL_FMT_H

#include <stdint.h>

/**
 * @brief  无符号整数转字符串（任意 2~16 进制）
 * @param  buf    输出缓冲
 * @param  cap    缓冲总大小（含 '\0'）
 * @param  v      数值
 * @param  base   进制 2~16，非法值按 10 处理
 * @param  width  最小宽度，不足左边补 '0'（0 = 不补）
 * @return 写入的字符数
 * @note   16 进制时输出大写 A~F；要 "0x" 前缀自己加
 */
static inline uint8_t tool_u32_str(char *buf, uint8_t cap, uint32_t v,
                                   uint8_t base, uint8_t width)
{
    char    tmp[33];
    uint8_t n     = 0;
    uint8_t total;
    uint8_t pos   = 0;
    uint8_t i;

    if((buf == 0) || (cap == 0u)) { return 0; }

    if((base < 2u) || (base > 16u)) { base = 10u; }

    do
    {
        uint8_t d = (uint8_t)(v % base);

        tmp[n++] = (d < 10u) ? (char)('0' + (int)d) : (char)('A' + (int)d - 10);
        v /= base;
    } while((v != 0u) && (n < 32u));

    total = (width > n) ? width : n;
    if(total > (uint8_t)(cap - 1u)) { total = (uint8_t)(cap - 1u); }

    while((total - pos) > n) { buf[pos++] = '0'; }     /* 左补零 */

    i = n;
    while((i > 0u) && (pos < total)) { buf[pos++] = tmp[--i]; }

    buf[pos] = '\0';

    return pos;
}

/**
 * @brief  有符号整数转字符串
 * @param  width  含符号位在内的最小宽度（0 = 不补）
 */
static inline uint8_t tool_i32_str(char *buf, uint8_t cap, int32_t v, uint8_t width)
{
    uint32_t mag;
    uint8_t  len;

    if((buf == 0) || (cap == 0u)) { return 0; }

    if(v >= 0)
    {
        return tool_u32_str(buf, cap, (uint32_t)v, 10u, width);
    }

    if(cap < 2u)
    {
        buf[0] = '\0';
        return 0;
    }

    buf[0] = '-';

    /* 先变成正数再取绝对值：直接 -v 在 INT32_MIN 时溢出 */
    mag = (uint32_t)(-(v + 1)) + 1u;

    len = tool_u32_str(buf + 1, (uint8_t)(cap - 1u), mag, 10u,
                       (width > 0u) ? (uint8_t)(width - 1u) : 0u);

    return (uint8_t)(len + 1u);
}

/**
 * @brief  定点数转字符串：value 是放大 10^dec 后的整数
 * @param  v    放大后的整数，例如 25.3 传 253
 * @param  dec  小数位数 1~4，传 0 等于 tool_i32_str
 * @return 写入的字符数
 * @note   位数不够会截断小数位；-0.5 这种会正确输出 "-0.5" 而不是 "-.5"
 */
static inline uint8_t tool_fx_str(char *buf, uint8_t cap, int32_t v, uint8_t dec)
{
    uint32_t mag;
    uint32_t div = 1;
    uint8_t  i;
    uint8_t  pos = 0;

    if((buf == 0) || (cap == 0u)) { return 0; }

    if((dec == 0u) || (dec > 4u)) { return tool_i32_str(buf, cap, v, 0u); }

    for(i = 0; i < dec; i++) { div *= 10u; }

    mag = (v < 0) ? ((uint32_t)(-(v + 1)) + 1u) : (uint32_t)v;

    if(v < 0)
    {
        if((uint8_t)(cap - 1u) == 0u)
        {
            buf[0] = '\0';
            return 0;
        }

        buf[pos++] = '-';
    }

    pos = (uint8_t)(pos + tool_u32_str(buf + pos, (uint8_t)(cap - pos),
                                       mag / div, 10u, 0u));

    if((uint8_t)(pos + 1u) < cap)
    {
        buf[pos++] = '.';
    }

    pos = (uint8_t)(pos + tool_u32_str(buf + pos, (uint8_t)(cap - pos),
                                       mag % div, 10u, dec));

    return pos;
}

/**
 * @brief  字节数组转大写 16 进制，字节之间一个空格
 * @param  src  数据，可传 0（返回空串）
 * @param  n    字节数
 * @note   调试打帧头最好用："AA 55 03 01"；cap 给不够会自动截断到整字节
 */
static inline uint8_t tool_bytes_str(char *buf, uint8_t cap,
                                     const uint8_t *src, uint16_t n)
{
    static const char hex[] = "0123456789ABCDEF";
    uint8_t  pos = 0;
    uint16_t i;

    if((buf == 0) || (cap == 0u)) { return 0; }

    if((src == 0) || (cap < 4u))
    {
        buf[0] = '\0';
        return 0;
    }

    for(i = 0; i < n; i++)
    {
        if((uint8_t)(pos + 3u) > (uint8_t)(cap - 1u)) { break; }   /* 2 字符 + 空格 + '\0' */

        if(i != 0u) { buf[pos++] = ' '; }

        buf[pos++] = hex[src[i] >> 4];
        buf[pos++] = hex[src[i] & 0x0Fu];
    }

    buf[pos] = '\0';

    return pos;
}

/**
 * @brief  秒 -> "MM:SS"（LCD 上显示倒计时/计时）
 * @param  total_sec  总秒数，3661 -> "61:01"
 * @note   分钟不截断（不会变成小时），赛题的计时最长也才 30 分钟
 */
static inline uint8_t tool_mmss_str(char *buf, uint8_t cap, uint32_t total_sec)
{
    uint8_t len;

    if((buf == 0) || (cap < 6u))
    {
        if((buf != 0) && (cap != 0u)) { buf[0] = '\0'; }
        return 0;
    }

    len = tool_u32_str(buf, cap, total_sec / 60u, 10u, 2u);

    if((uint8_t)(len + 1u) >= cap) { return len; }

    buf[len++] = ':';

    len = (uint8_t)(len + tool_u32_str(buf + len, (uint8_t)(cap - len),
                                       total_sec % 60u, 10u, 2u));

    return len;
}

/**
 * @brief  分、秒 -> 4 位数（给数码管用）
 * @param  min  分钟 0~99
 * @param  sec  秒 0~59
 * @return min*100 + sec；配 ch422_show_number_dp(v, 1) 就显示成 "12.34"
 * @note   赛题要求的 "XX.YY" 就是这个格式，小数点位置由 dp_index 决定
 */
static inline uint16_t tool_minsec4(uint8_t min, uint8_t sec)
{
    return (uint16_t)(((uint16_t)(min % 100u) * 100u) + (sec % 100u));
}

#endif
