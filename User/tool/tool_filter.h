/******************************************************************************
 * 文件名   : tool_filter.h
 * 说明     : 一维滤波 —— 指数滑动平均 / 中值 / 滑动平均 / 迟滞 / 死区
 *
 * 传感器和麦克风读数都在"抖"，直接上屏就是一条毛刺曲线。
 * 这里全是定点的、无状态的（除了两个小结构体），不占堆、不进中断。
 *
 * 用法:
 *     #include "tool/tool.h"
 *
 *     static int32_t s_lux;
 *     lux = tool_ema_step(&s_lux, veml7700_lux_get(), 3);   // 8 级平滑
 *
 *     static tool_ma_t s_ma;
 *     tool_ma_init(&s_ma, 8);
 *     avg = tool_ma_push(&s_ma, mic_amp(512));
 ******************************************************************************/

#ifndef __TOOL_FILTER_H
#define __TOOL_FILTER_H

#include <stdint.h>

#ifndef TOOL_MA_MAX
#define TOOL_MA_MAX         16      /* 滑动平均最多几阶；越大越平滑越迟钝 */
#endif

/*********************************************************************
 * 指数滑动平均（EMA）
 *********************************************************************/

/**
 * @brief  一阶低通：y += (x - y) / 2^k
 * @param  state  上次的输出（首次调用前先给它赋一个合理初值）
 * @param  x      这次的采样值
 * @param  k      平滑系数 0~15，0 = 不滤波，1 = 很跟手，4~6 = 很平滑
 * @return 滤波后的值
 * @note   和滑动平均比：只占 4 字节、不用缓冲；阶跃响应是"慢慢爬"而不是取窗口
 */
static inline int32_t tool_ema_step(int32_t *state, int32_t x, uint8_t k)
{
    if(k == 0u)
    {
        *state = x;
        return x;
    }

    if(k > 15u) { k = 15u; }

    *state += (x - *state) >> k;

    return *state;
}

/*********************************************************************
 * 滑动平均（固定窗口，环形覆盖）
 *********************************************************************/
typedef struct
{
    int32_t  buf[TOOL_MA_MAX];
    int32_t  sum;
    uint8_t  n;             /* 实际窗口长度 */
    uint8_t  idx;           /* 下一个要覆盖的位置 */
    uint8_t  cnt;           /* 已经填了几个（没填满时按实际个数平均） */
} tool_ma_t;

/**
 * @brief  初始化滑动平均
 * @param  n  窗口长度 1~TOOL_MA_MAX
 */
static inline void tool_ma_init(tool_ma_t *ma, uint8_t n)
{
    uint8_t i;

    if(n == 0u) { n = 1u; }
    if(n > TOOL_MA_MAX) { n = TOOL_MA_MAX; }

    for(i = 0; i < TOOL_MA_MAX; i++) { ma->buf[i] = 0; }

    ma->sum = 0;
    ma->n   = n;
    ma->idx = 0;
    ma->cnt = 0;
}

/**
 * @brief  塞一个新值，返回当前窗口平均
 * @note   前 n-1 次返回的是"已填个数"的平均，曲线不会从 0 慢慢爬上来
 */
static inline int32_t tool_ma_push(tool_ma_t *ma, int32_t v)
{
    if((ma->n == 0u) || (ma->n > TOOL_MA_MAX))     /* 忘了 init 也别崩在除零上 */
    {
        tool_ma_init(ma, TOOL_MA_MAX);
    }

    ma->sum -= ma->buf[ma->idx];

    ma->buf[ma->idx] = v;
    ma->sum += v;

    ma->idx = (uint8_t)((ma->idx + 1u) % ma->n);
    if(ma->cnt < ma->n) { ma->cnt++; }

    return ma->sum / ma->cnt;
}

/*********************************************************************
 * 其它小工具
 *********************************************************************/

/**
 * @brief  三数取中：去掉单点毛刺（按键/冲击检测最省事的一招）
 */
static inline int32_t tool_median3(int32_t a, int32_t b, int32_t c)
{
    if(a > b) { int32_t t = a; a = b; b = t; }
    if(b > c) { b = c; }
    if(a > b) { b = a; }

    return b;
}

/**
 * @brief  数组平均（采样点数少、不想开结构体时用）
 * @param  a  无符号采样数组
 * @param  n  个数，0 返回 0
 * @note   n × 最大值 不能超过 2^31，4096 点 × 4095 没问题
 */
static inline int32_t tool_avg_u16(const uint16_t *a, uint16_t n)
{
    uint32_t s = 0;
    uint16_t i;

    if((a == 0) || (n == 0u)) { return 0; }

    for(i = 0; i < n; i++) { s += a[i]; }

    return (int32_t)(s / n);
}

/**
 * @brief  一次求最小 / 最大（自动量程、曲线缩放的前置步骤）
 * @param  vmin  返回最小值，可传 0
 * @param  vmax  返回最大值，可传 0
 * @note   数组为空时 vmin/vmax 都返回 0
 */
static inline void tool_minmax_u16(const uint16_t *a, uint16_t n,
                                   uint16_t *vmin, uint16_t *vmax)
{
    uint16_t i;
    uint16_t lo;
    uint16_t hi;

    if((a == 0) || (n == 0u))
    {
        if(vmin != 0) { *vmin = 0; }
        if(vmax != 0) { *vmax = 0; }
        return;
    }

    lo = a[0];
    hi = a[0];

    for(i = 1; i < n; i++)
    {
        if(a[i] < lo) { lo = a[i]; }
        if(a[i] > hi) { hi = a[i]; }
    }

    if(vmin != 0) { *vmin = lo; }
    if(vmax != 0) { *vmax = hi; }
}

/*********************************************************************
 * 迟滞开关（涨到 hi 才开，掉到 lo 才关）
 *
 * 阈值检测直接用一个门限，噪声在门限附近来回摆就会疯狂开关：
 *     if(mic_amp(64) > 300) led_on();
 * 改成迟滞：超过 400 开、低于 250 才关，中间留 150 的死区。
 *********************************************************************/
typedef struct
{
    int32_t lo;             /* 关断门限 */
    int32_t hi;             /* 打开门限 */
    uint8_t on;             /* 当前状态 */
} tool_hyst_t;

/**
 * @brief  初始化迟滞（lo 必须小于 hi）
 */
static inline void tool_hyst_init(tool_hyst_t *h, int32_t lo, int32_t hi)
{
    if(lo > hi) { int32_t t = lo; lo = hi; hi = t; }

    h->lo = lo;
    h->hi = hi;
    h->on = 0;
}

/**
 * @brief  喂一个值，返回当前开关状态（0/1）
 */
static inline uint8_t tool_hyst_step(tool_hyst_t *h, int32_t v)
{
    if(h->on != 0u)
    {
        if(v < h->lo) { h->on = 0; }
    }
    else
    {
        if(v > h->hi) { h->on = 1; }
    }

    return h->on;
}

/**
 * @brief  死区：绝对值小于 band 一律输出 0，外面则减去 band
 * @note   摇杆 / 编码器回中、电机微抖抑制
 */
static inline int32_t tool_deadband(int32_t v, int32_t band)
{
    if(band < 0) { band = -band; }

    if(v > band)  { return v - band; }
    if(v < -band) { return v + band; }

    return 0;
}

/**
 * @brief  变化率（每个采样点变化多少），正数在涨、负数在跌
 * @note   给"趋势箭头"用：tool_slope(prev, now) > 0 就是往上走
 */
static inline int32_t tool_slope(int32_t prev, int32_t now)
{
    return now - prev;
}

#endif
