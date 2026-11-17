/******************************************************************************
 * 文件名   : tool_seq.h
 * 说明     : 非阻塞序列播放 —— "闪 3 次""鸣 N 声""流水步进"全用它
 *
 * 赛题里到处是这类要求：
 *   · 任务 2：青紫交替闪烁 3 次后关闭；蜂鸣器鸣叫次数 = 分钟值
 *   · 任务 8：状态灯按 等待→抓拍→完成 三段闪烁
 * 这些如果写成 delay()，UI 直接卡死；写成一堆分频计数器又会越写越乱。
 *
 * 做法：把"亮多久、灭多久"排成一张表，poll() 每 10ms 喂一次当前时间，
 *      它返回这一刻该输出的电平（1 = 亮/响，0 = 灭/停）。
 *
 * 用法:
 *     static const tool_step_t blink3[] = {
 *         {1, 200}, {0, 200}, {1, 200}, {0, 200}, {1, 200}, {0, 200},
 *     };
 *     tool_seq_start(&s_seq, blink3, 6, now);
 *     if(tool_seq_poll(&s_seq, now)) led_on(); else led_off();
 ******************************************************************************/

#ifndef __TOOL_SEQ_H
#define __TOOL_SEQ_H

#include <stdint.h>

/* 序列里的一步：电平（1 = 亮/响）+ 持续毫秒 */
typedef struct
{
    uint8_t  on;
    uint16_t ms;
} tool_step_t;

typedef struct
{
    const tool_step_t *tab;         /* 步表（常驻内存，一般 static const 放 Flash） */
    uint16_t n;                     /* 一共几步 */
    uint16_t idx;                   /* 当前走到第几步 */
    uint32_t t0;                    /* 当前步的起始时间戳 */
    uint8_t  level;                 /* 当前电平 */
    uint8_t  active;                /* 1 = 还在播 */
} tool_seq_t;

/**
 * @brief  开始播放（从第一步开始，立即生效）
 */
static inline void tool_seq_start(tool_seq_t *s, const tool_step_t *tab,
                                  uint16_t n, uint32_t now_ms)
{
    s->tab    = tab;
    s->n      = n;
    s->idx    = 0;
    s->t0     = now_ms;
    s->active = (uint8_t)((tab != 0) && (n != 0u));
    s->level  = (s->active != 0u) ? tab[0].on : 0u;
}

/**
 * @brief  停掉（输出立刻回 0）
 */
static inline void tool_seq_stop(tool_seq_t *s)
{
    s->active = 0;
    s->level  = 0;
}

/**
 * @brief  还在播吗
 */
static inline uint8_t tool_seq_busy(const tool_seq_t *s)
{
    return (s != 0) ? s->active : 0u;
}

/**
 * @brief  每 10ms 调一次，返回这一刻应该输出的电平
 * @return 1 = 亮/响，0 = 灭/停（序列结束后恒为 0）
 * @note   t0 用"累加步长"推进，不是直接赋 now，所以调用间隔抖动不会越积越多；
 *         一步时长为 0 时直接跳过，不会死循环
 */
static inline uint8_t tool_seq_poll(tool_seq_t *s, uint32_t now_ms)
{
    if((s == 0) || (s->active == 0u)) { return 0u; }

    while((uint32_t)(now_ms - s->t0) >= s->tab[s->idx].ms)
    {
        uint16_t ms = s->tab[s->idx].ms;

        if(ms == 0u) { s->t0 = now_ms; }        /* 防御：0 时长直接跳过 */
        else         { s->t0 += ms; }

        s->idx++;

        if(s->idx >= s->n)
        {
            s->active = 0;
            s->level  = 0;
            return 0u;
        }

        s->level = s->tab[s->idx].on;

        if(ms == 0u) { break; }                 /* 别被一串 0 卡住 */
    }

    return s->level;
}

/**
 * @brief  最省事的闪烁：不需要状态，按时间算方波
 * @param  now_ms   当前时间戳
 * @param  half_ms  半个周期（亮 half_ms、灭 half_ms）
 * @return 1 = 亮，0 = 灭
 * @note   任务 2 的"红黄快速交替"直接：
 *             led_red(tool_blink(now, 100) != 0);
 *             led_yellow(tool_blink(now + 100, 100) != 0);   // 错开半拍
 */
static inline uint8_t tool_blink(uint32_t now_ms, uint16_t half_ms)
{
    if(half_ms == 0u) { return 1u; }

    return (uint8_t)(((now_ms / half_ms) & 1u) ? 0u : 1u);   /* 先亮后灭 */
}

#endif
