/******************************************************************************
 * 文件名   : tool_color.h
 * 说明     : 颜色 —— RGB 拼装/拆分、HSV 取色、渐变、RGB565
 *
 * 赛题里 RGB 彩灯、灯带流水、UI 选中态都要颜色运算：
 *   · 任务 2「红黄交替 / 青紫闪 3 次 / 流水灯」→ tool_wheel() + tool_rgb_lerp()
 *   · UI 按钮激活态变色 → tool_rgb_scale()
 *   · LVGL 画点画线要 RGB565 → tool_rgb565()
 *
 * 统一用 0xRRGGBB 打包（和 theme.h 里的 UI_C_* 一个格式），
 * 需要拆开时用 tool_rgb_split()。
 *
 * 用法:
 *     uint32_t c = tool_wheel(hue, 255);          // 彩虹流水，hue 每步 +10
 *     uint32_t g = tool_rgb_lerp(red, yellow, 50); // 红黄中间色
 *     lv_color_t lc = lv_color_hex(tool_rgb_scale(g, 60));   // 压暗 60%
 ******************************************************************************/

#ifndef __TOOL_COLOR_H
#define __TOOL_COLOR_H

#include <stdint.h>

/* 0xRRGGBB 打包 */
#define TOOL_RGB(r, g, b)       (((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b))

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} tool_rgb_t;

/**
 * @brief  拆成 r/g/b 三个分量
 */
static inline tool_rgb_t tool_rgb_split(uint32_t c)
{
    tool_rgb_t o;

    o.r = (uint8_t)((c >> 16) & 0xFFu);
    o.g = (uint8_t)((c >> 8) & 0xFFu);
    o.b = (uint8_t)(c & 0xFFu);

    return o;
}

/**
 * @brief  两色按百分比混合
 * @param  pct  0 = 全 a，100 = 全 b
 */
static inline uint32_t tool_rgb_lerp(uint32_t a, uint32_t b, uint8_t pct)
{
    tool_rgb_t ca;
    tool_rgb_t cb;

    if(pct > 100u) { pct = 100u; }

    ca = tool_rgb_split(a);
    cb = tool_rgb_split(b);

    return TOOL_RGB((uint8_t)(ca.r + ((cb.r - ca.r) * (int32_t)pct) / 100),
                    (uint8_t)(ca.g + ((cb.g - ca.g) * (int32_t)pct) / 100),
                    (uint8_t)(ca.b + ((cb.b - ca.b) * (int32_t)pct) / 100));
}

/**
 * @brief  整体压暗/提亮
 * @param  pct  0 = 全黑，100 = 原色，>100 会往白色方向夹住
 * @note   灯带做"呼吸"效果：亮度按 0→100→0 来回变，颜色不用换
 */
static inline uint32_t tool_rgb_scale(uint32_t c, uint8_t pct)
{
    tool_rgb_t t = tool_rgb_split(c);
    int32_t   r;
    int32_t   g;
    int32_t   b;

    r = (t.r * (int32_t)pct) / 100;
    g = (t.g * (int32_t)pct) / 100;
    b = (t.b * (int32_t)pct) / 100;

    if(r > 255) { r = 255; }
    if(g > 255) { g = 255; }
    if(b > 255) { b = 255; }

    return TOOL_RGB((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

/**
 * @brief  0xRRGGBB -> RGB565 原始值
 * @note   LVGL 用 lv_color_t，两种写法都行：
 *             lv_color_t c = lv_color_hex(rgb888);
 *             lv_color_t c = {.full = tool_rgb565(rgb888)};
 *         直接写 canvas 的 buf / 手搓 framebuffer 时用后者
 */
static inline uint16_t tool_rgb565(uint32_t c)
{
    tool_rgb_t t = tool_rgb_split(c);

    return (uint16_t)(((uint16_t)(t.r & 0xF8u) << 8) |
                      ((uint16_t)(t.g & 0xFCu) << 3) |
                      ((uint16_t)t.b >> 3));
}

/**
 * @brief  HSV 转 RGB
 * @param  h  色相 0~359（0 红、120 绿、240 蓝）
 * @param  s  饱和度 0~255
 * @param  v  亮度 0~255
 * @return 0xRRGGBB
 * @note   整数实现，没有浮点。s=0 是灰阶，v=0 是黑
 */
static inline uint32_t tool_hsv2rgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint8_t  region;
    uint8_t  rem;
    uint8_t  p;
    uint8_t  q;
    uint8_t  t;

    h %= 360u;

    region = (uint8_t)(h / 60u);
    rem    = (uint8_t)(((uint32_t)(h % 60u) * 255u) / 60u);

    /* p = v*(1-s)，q = v*(1-s*rem)，t = v*(1-s*(1-rem))，全是 0~255 定点 */
    p = (uint8_t)(((uint32_t)v * (255u - s)) / 255u);
    q = (uint8_t)(((uint32_t)v * (255u - (((uint32_t)s * rem) / 255u))) / 255u);
    t = (uint8_t)(((uint32_t)v * (255u - (((uint32_t)s * (255u - rem)) / 255u))) / 255u);

    switch(region)
    {
        case 0u:  return TOOL_RGB(v, t, p);
        case 1u:  return TOOL_RGB(q, v, p);
        case 2u:  return TOOL_RGB(p, v, t);
        case 3u:  return TOOL_RGB(p, q, v);
        case 4u:  return TOOL_RGB(t, p, v);
        default:  return TOOL_RGB(v, p, q);
    }
}

/**
 * @brief  色环取色：第 index 颗灯珠什么颜色（彩虹流水）
 * @param  index  第几颗（0 ~ total-1）
 * @param  total  一共几颗
 * @param  v      亮度 0~255
 * @note   灯带"从左到右流水"：index 每帧 +1，超出 total 就回绕
 */
static inline uint32_t tool_wheel(uint16_t index, uint16_t total, uint8_t v)
{
    uint16_t hue;

    if(total == 0u) { total = 1u; }

    hue = (uint16_t)(((uint32_t)(index % total) * 360u) / total);

    return tool_hsv2rgb(hue, 255u, v);
}

/**
 * @brief  色相平移（红->黄->绿 循环）
 * @param  hue     原色相 0~359
 * @param  offset  偏移度数，可正可负
 */
static inline uint16_t tool_hue_shift(uint16_t hue, int16_t offset)
{
    int32_t h = (int32_t)(hue % 360u) + offset;

    while(h < 0)    { h += 360; }
    while(h >= 360) { h -= 360; }

    return (uint16_t)h;
}

#endif
