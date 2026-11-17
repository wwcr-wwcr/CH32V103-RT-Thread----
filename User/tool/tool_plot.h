/******************************************************************************
 * 文件名   : tool_plot.h
 * 说明     : 曲线自动量程 —— 给 ui/common.h 的 ui_plot_t 配量程用
 *
 * ui_plot_line() 要的是固定 vmin/vmax。传感器量程差得很远：
 *     照度：室内 50 lux，窗边 3000 lux
 *     麦克风峰峰值：安静 30，拍手 1500
 * 写死一个量程，要么贴底要么削顶。这里的做法是：
 *     1) 用 tool_range_t 一路攒 min/max（tool_range_add）
 *     2) 到 UI 里用 tool_range_auto() 换成"好看"的上下限（1/2/5×10^n）
 *     3) 只有上下限变了才调 ui_plot_line()，避免每帧重配
 *
 * 用法:
 *     static tool_range_t s_rng;          // 逻辑线程攒
 *     tool_range_add(&s_rng, lux);
 *
 *     int32_t lo, hi;                     // UI 线程换算
 *     tool_range_auto(&s_rng, 4, &lo, &hi);
 *     if((lo != s_lo) || (hi != s_hi))
 *     {
 *         s_lo = lo; s_hi = hi;
 *         ui_plot_line(s_plot, 1, &(ui_plot_line_t){ .color = ..., .vmin = lo, .vmax = hi });
 *     }
 ******************************************************************************/

#ifndef __TOOL_PLOT_H
#define __TOOL_PLOT_H

#include <stdint.h>

typedef struct
{
    int32_t lo;             /* 见过的最小值 */
    int32_t hi;             /* 见过的最大值 */
    uint8_t has;            /* 攒过数据没有（没有就别拿去算量程） */
} tool_range_t;

/**
 * @brief  清空，重新开始攒
 */
static inline void tool_range_reset(tool_range_t *r)
{
    r->lo  = 0x7FFFFFFF;
    r->hi  = (int32_t)0x80000000u;
    r->has = 0;
}

/**
 * @brief  塞一个采样值进去（只更新 min/max，不存数据）
 */
static inline void tool_range_add(tool_range_t *r, int32_t v)
{
    if(v < r->lo) { r->lo = v; }
    if(v > r->hi) { r->hi = v; }

    r->has = 1;
}

/**
 * @brief  当前跨度（最大值 - 最小值），没数据返回 0
 */
static inline int32_t tool_range_span(const tool_range_t *r)
{
    if((r == 0) || (r->has == 0u)) { return 0; }

    return r->hi - r->lo;
}

/**
 * @brief  把任意数凑成 1/2/5 × 10^n 这种"人看着舒服"的刻度
 * @param  v   原始值
 * @param  up  1 = 向上取（做刻度间距），0 = 向下取（做步长）
 * @note   up: 7->10、37->50、100->100；down: 7->5、37->20、100->100
 */
static inline int32_t tool_nice_num(int32_t v, uint8_t up)
{
    int32_t p = 1;
    int32_t r;

    if(v < 0)
    {
        r = tool_nice_num(-v, up);
        return -r;
    }

    if(v == 0) { return 0; }

    while(p <= (v / 10)) { p *= 10; }       /* p = 不超过 v 的最大 10 的幂 */

    if(up == 0u)
    {
        if(v >= (p * 5)) { return p * 5; }
        if(v >= (p * 2)) { return p * 2; }
        return p;
    }

    if(v <= p)       { return p; }
    if(v <= (p * 2)) { return p * 2; }
    if(v <= (p * 5)) { return p * 5; }

    return p * 10;
}

/**
 * @brief  根据攒到的 min/max 算出 ui_plot 该用的 vmin/vmax
 * @param  r      tool_range_t（没数据时输出 0~100）
 * @param  div    纵轴想分几格（一般 4，和 ui_plot_axis 的 y_div 一致）
 * @param  p_lo   输出下限
 * @param  p_hi   输出上限
 * @note   上下限一定落在刻度线上；跨度比原始数据略大，曲线不会贴边
 */
static inline void tool_range_auto(const tool_range_t *r, uint8_t div,
                                   int32_t *p_lo, int32_t *p_hi)
{
    int32_t lo;
    int32_t hi;
    int32_t step;

    if((p_lo == 0) || (p_hi == 0)) { return; }

    if((r == 0) || (r->has == 0u))
    {
        *p_lo = 0;
        *p_hi = 100;
        return;
    }

    lo = r->lo;
    hi = r->hi;

    if(hi <= lo) { hi = lo + 1; }           /* 一条直线也要有个跨度 */
    if(div == 0u) { div = 4u; }

    step = tool_nice_num((hi - lo) / (int32_t)div, 1u);
    if(step < 1) { step = 1; }

    /* lo 向下对齐、hi 向上对齐到 step 的整数倍（负数要单独算，C 的除法是向零截断） */
    lo = (lo >= 0) ? ((lo / step) * step)
                   : (-(((-lo) + step - 1) / step) * step);

    hi = (hi >= 0) ? (((hi + step - 1) / step) * step)
                   : (-(((-hi) / step) * step));

    if(hi <= lo) { hi = lo + step; }

    *p_lo = lo;
    *p_hi = hi;
}

#endif
