/******************************************************************************
 * 文件名   : tool_timer.h
 * 说明     : 非阻塞软件定时器 —— 替代 `if(++div >= N)` 这种手工分频
 *
 * 任务里的 update() 是 10ms 一次（menu.c 的 UI_PERIOD_MS），
 * 需要"每 1 秒做一次"的时候有两套写法：
 *
 *   手工分频（现在的 task_example.c 就是这种）：
 *       if(++s_div >= 100) { s_div = 0; do_something(); }
 *       问题：改周期要改宏；多个周期要多个计数器；tick 不准时误差会累积
 *
 *   软件定时器（本文件）：
 *       static tool_timer_t t;
 *       if(tool_timer_tick(&t, now)) { do_something(); }
 *       周期是毫秒、随手可改；基于时间戳，不累积误差
 *
 * 时间基准从哪来（本文件不绑死任何 OS）：
 *     RT-Thread ：rt_tick_get_millisecond()
 *     裸机      ：自己维护一个 1ms 自增的 uint32_t，在 SysTick 里 ++
 *
 * 全部用无符号减法 (now - last)，32 位毫秒计数回绕（约 49.7 天）也不会出错。
 ******************************************************************************/

#ifndef __TOOL_TIMER_H
#define __TOOL_TIMER_H

#include <stdint.h>

typedef struct
{
    uint32_t last;          /* 上次触发的时间戳（ms） */
    uint32_t period;        /* 周期（ms） */
} tool_timer_t;

/**
 * @brief  设定周期并立即从"现在"重新计时
 * @param  now_ms  当前毫秒时间戳
 */
static inline void tool_timer_set(tool_timer_t *t, uint32_t now_ms, uint32_t period_ms)
{
    t->last   = now_ms;
    t->period = period_ms;
}

/**
 * @brief  只问不触发：到期了没有
 * @return 1 = 已经到期，0 = 还没到
 * @note   想"到点后再做点别的事"又不想重置计时器时用它
 */
static inline uint8_t tool_timer_peek(const tool_timer_t *t, uint32_t now_ms)
{
    return ((uint32_t)(now_ms - t->last) >= t->period) ? 1u : 0u;
}

/**
 * @brief  到期就自动重置，返回 1
 * @return 1 = 这一拍该干活了；0 = 还没到
 * @note   重置用的是 last = now，不是 last += period：
 *         万一线程被卡了 500ms，不会一口气补发 50 次（那种"补拍"在 UI 里是灾难）
 */
static inline uint8_t tool_timer_tick(tool_timer_t *t, uint32_t now_ms)
{
    if((uint32_t)(now_ms - t->last) < t->period) { return 0u; }

    t->last = now_ms;

    return 1u;
}

/**
 * @brief  手动重新计时（比如按键后想让周期从头开始）
 */
static inline void tool_timer_reset(tool_timer_t *t, uint32_t now_ms)
{
    t->last = now_ms;
}

/**
 * @brief  从现在开始算，过了多久（ms），自动处理回绕
 */
static inline uint32_t tool_elapsed(uint32_t start_ms, uint32_t now_ms)
{
    return (uint32_t)(now_ms - start_ms);
}

/**
 * @brief  从某个时间点起，是否已经过了 ms 毫秒
 * @note   一次性延时的最简写法：
 *             s_t0 = now;
 *             ...
 *             if(tool_since(s_t0, now, 3000)) { 三秒到了 }
 */
static inline uint8_t tool_since(uint32_t start_ms, uint32_t now_ms, uint32_t ms)
{
    return ((uint32_t)(now_ms - start_ms) >= ms) ? 1u : 0u;
}

#endif
