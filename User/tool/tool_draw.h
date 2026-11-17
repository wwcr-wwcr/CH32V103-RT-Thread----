/******************************************************************************
 * 文件名   : tool_draw.h
 * 说明     : 逐点绘图算法 —— 直线 / 圆 / 圆弧 / 指针，全用回调吐坐标
 *
 * 对应赛题：
 *   · 任务 3「触控绘图」：手指划过的点要连成线，还要橡皮擦（实心圆）
 *   · 任务 4「转速指针」：圆弧刻度 + 指针端点
 *   · 任务 6「波形绘制」：把采样点连成折线
 *
 * 为什么不直接调 LVGL 的 lv_draw_line：这两件事不一样。
 * lv_draw_line 在 draw 事件里画；而要"把轨迹存下来"（保存 2~3 张图）、
 * 或者往 canvas / 位图缓冲里写点，必须自己逐点算坐标。
 * 这里每个函数都是"算一个点就回调一次"，你在回调里写 canvas 也好、
 * 置位图也好、喂给 lv_canvas_set_px 也好，都不受约束。
 *
 * 用法:
 *     static void put_px(int16_t x, int16_t y, void *user)
 *     {
 *         lv_canvas_set_px((lv_obj_t *)user, x, y, lv_color_white());
 *     }
 *     tool_line_each(x0, y0, x1, y1, put_px, canvas);
 ******************************************************************************/

#ifndef __TOOL_DRAW_H
#define __TOOL_DRAW_H

#include <stdint.h>
#include "tool_wave.h"          /* tool_sin_q15 / tool_cos_q15 */

/* 每个点回调一次；user 原样透传，一般放 canvas 或位图缓冲指针 */
typedef void (*tool_point_cb)(int16_t x, int16_t y, void *user);

/*********************************************************************
 * 内部小工具
 *********************************************************************/
static inline int16_t tool_abs16(int16_t v)
{
    return (v < 0) ? (int16_t)(-v) : v;
}

/* 水平线段（实心圆复用） */
static inline void tool_hspan(tool_point_cb cb, void *user,
                              int16_t x0, int16_t x1, int16_t y)
{
    int16_t x;

    if(x0 > x1) { int16_t t = x0; x0 = x1; x1 = t; }

    for(x = x0; x <= x1; x++) { cb(x, y, user); }
}

/*********************************************************************
 * 直线
 *********************************************************************/

/**
 * @brief  画一条直线（Bresenham，逐点回调）
 * @note   8 个方向都正确，不会漏点也不会重复点；
 *         触控画线时把上一个点到当前点连一次即可，不用自己算斜率
 */
static inline void tool_line_each(int16_t x0, int16_t y0, int16_t x1, int16_t y1,
                                  tool_point_cb cb, void *user)
{
    int16_t dx;
    int16_t dy;
    int16_t sx;
    int16_t sy;
    int16_t err;

    if(cb == 0) { return; }

    dx  = (int16_t)tool_abs16((int16_t)(x1 - x0));
    dy  = (int16_t)-tool_abs16((int16_t)(y1 - y0));
    sx  = (x0 < x1) ? 1 : -1;
    sy  = (y0 < y1) ? 1 : -1;
    err = (int16_t)(dx + dy);

    for(;;)
    {
        int16_t e2;

        cb(x0, y0, user);

        if((x0 == x1) && (y0 == y1)) { break; }

        e2 = (int16_t)(err * 2);

        if(e2 >= dy) { err = (int16_t)(err + dy); x0 = (int16_t)(x0 + sx); }
        if(e2 <= dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/*********************************************************************
 * 圆
 *********************************************************************/

/**
 * @brief  画圆轮廓（中点画圆法，逐点回调）
 * @param  r  半径，0 时只回调圆心一次
 */
static inline void tool_circle_each(int16_t cx, int16_t cy, int16_t r,
                                    tool_point_cb cb, void *user)
{
    int16_t x;
    int16_t y;
    int16_t err;

    if(cb == 0) { return; }
    if(r <= 0)  { cb(cx, cy, user); return; }

    x   = r;
    y   = 0;
    err = (int16_t)(1 - r);

    while(x >= y)
    {
        cb((int16_t)(cx + x), (int16_t)(cy + y), user);
        cb((int16_t)(cx - x), (int16_t)(cy + y), user);
        cb((int16_t)(cx + x), (int16_t)(cy - y), user);
        cb((int16_t)(cx - x), (int16_t)(cy - y), user);
        cb((int16_t)(cx + y), (int16_t)(cy + x), user);
        cb((int16_t)(cx - y), (int16_t)(cy + x), user);
        cb((int16_t)(cx + y), (int16_t)(cy - x), user);
        cb((int16_t)(cx - y), (int16_t)(cy - x), user);

        y++;

        if(err < 0)
        {
            err = (int16_t)(err + (2 * y) + 1);
        }
        else
        {
            x--;
            err = (int16_t)(err + (2 * (y - x)) + 1);
        }
    }
}

/**
 * @brief  画实心圆（橡皮擦 / 笔刷 / 圆点按钮）
 * @note   每行画一条水平线段，半径 20 的圆大约 40 次回调，比逐点判距快得多
 */
static inline void tool_circle_fill_each(int16_t cx, int16_t cy, int16_t r,
                                         tool_point_cb cb, void *user)
{
    int16_t x;
    int16_t y;
    int16_t err;

    if(cb == 0) { return; }
    if(r <= 0)  { cb(cx, cy, user); return; }

    x   = r;
    y   = 0;
    err = (int16_t)(1 - r);

    while(x >= y)
    {
        tool_hspan(cb, user, (int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy + y));
        tool_hspan(cb, user, (int16_t)(cx - x), (int16_t)(cx + x), (int16_t)(cy - y));
        tool_hspan(cb, user, (int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy + x));
        tool_hspan(cb, user, (int16_t)(cx - y), (int16_t)(cx + y), (int16_t)(cy - x));

        y++;

        if(err < 0)
        {
            err = (int16_t)(err + (2 * y) + 1);
        }
        else
        {
            x--;
            err = (int16_t)(err + (2 * (y - x)) + 1);
        }
    }
}

/*********************************************************************
 * 圆弧 / 指针
 *********************************************************************/

/**
 * @brief  画一段圆弧，1 度一个点
 * @param  deg0  起始角度（度），0 = 正右方，顺时针为正
 * @param  deg1  结束角度
 * @note   表盘刻度圈：tool_arc_each(cx, cy, r, 225, 315, cb, user)
 */
static inline void tool_arc_each(int16_t cx, int16_t cy, int16_t r,
                                 int16_t deg0, int16_t deg1,
                                 tool_point_cb cb, void *user)
{
    int16_t d;

    if(cb == 0) { return; }
    if(deg1 < deg0) { int16_t t = deg0; deg0 = deg1; deg1 = t; }

    for(d = deg0; d <= deg1; d++)
    {
        int32_t x = (int32_t)cx + (((int32_t)tool_cos_q15(d) * r) >> 15);
        int32_t y = (int32_t)cy - (((int32_t)tool_sin_q15(d) * r) >> 15);

        cb((int16_t)x, (int16_t)y, user);
    }
}

/**
 * @brief  指针端点：给圆心、长度、角度，算针尖坐标（屏幕坐标，y 向下）
 * @note   转速表最关键的一行：
 *             tool_needle(cx, cy, len, tool_map(rpm, 0, 500, 225, -45), &px, &py);
 *             tool_line_each(cx, cy, px, py, put_px, canvas);
 */
static inline void tool_needle(int16_t cx, int16_t cy, int16_t len, int16_t deg,
                               int16_t *px, int16_t *py)
{
    int32_t x = (int32_t)cx + (((int32_t)tool_cos_q15(deg) * len) >> 15);
    int32_t y = (int32_t)cy - (((int32_t)tool_sin_q15(deg) * len) >> 15);

    if(px != 0) { *px = (int16_t)x; }
    if(py != 0) { *py = (int16_t)y; }
}

/**
 * @brief  两点距离的平方（不用开方，比大小够用）
 * @note   触控画线抽稀：距离小于 3px 的点丢掉，不然一个点会画出十几个点，
 *         既慢又费存储
 */
static inline uint32_t tool_dist2(int16_t x0, int16_t y0, int16_t x1, int16_t y1)
{
    int32_t dx = (int32_t)x1 - x0;
    int32_t dy = (int32_t)y1 - y0;

    return (uint32_t)(dx * dx + dy * dy);
}

#endif
