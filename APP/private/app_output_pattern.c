#include "app_output_pattern.h"

int app_output_durations(uint32_t first_ms, uint32_t second_ms,
                         app_output_phase_t *first, app_output_phase_t *second)
{
    uint64_t a = ((uint64_t)first_ms * RT_TICK_PER_SECOND + 999U) / 1000U;
    uint64_t b = ((uint64_t)second_ms * RT_TICK_PER_SECOND + 999U) / 1000U;
    /* 首段额外预留一个节拍，补偿启动时正在进行的部分节拍。 */
    if (!a || a + 1U >= RT_TICK_MAX / 2U || b >= RT_TICK_MAX / 2U) return 0;
    first->ticks = (rt_tick_t)a;
    second->ticks = (rt_tick_t)b;
    return 1;
}

/* 调用者已屏蔽中断。序列完全关闭后才释放短脉冲的时基保留。 */
static void finish(app_output_pattern_t *p)
{
    if (p->remaining) (void)rt_timer_stop(&p->timer);
    p->remaining = 0;
    p->stopping = 0;
    if (p->done) p->done();
}

static app_status_t stop_output(app_output_pattern_t *p, int in_callback)
{
    app_status_t rc = p->write(0);
    if (rc == APP_OK) {
        finish(p);
    } else {
        rt_tick_t retry = 1;
        p->remaining = 1;
        p->stopping = 1;
        rt_timer_control(&p->timer, RT_TIMER_CTRL_SET_TIME, &retry);
        /* 回调返回时内核自动按新周期重装。显式 stop 或启动回滚需自行安排重试。 */
        if (!in_callback) (void)rt_timer_start(&p->timer);
    }
    return rc;
}

static void next_phase(void *context)
{
    app_output_pattern_t *p = context;
    if (p->stopping || ((p->index || !p->phase[1].level) && p->remaining == 1)) {
        (void)stop_output(p, 1);
        return;
    }
    if (!p->index && !p->phase[1].ticks) {
        --p->remaining;
        /* 零间隔保持连续输出。只有第一个窗口需要补偿部分节拍。 */
        rt_timer_control(&p->timer, RT_TIMER_CTRL_SET_TIME, &p->phase[0].ticks);
        return;
    }
    if (p->index) --p->remaining;
    p->index ^= 1U;
    app_output_phase_t *next = &p->phase[p->index];
    if (p->write(next->level) != APP_OK ||
        rt_timer_control(&p->timer, RT_TIMER_CTRL_SET_TIME, &next->ticks) != RT_EOK)
        (void)stop_output(p, 1);
}

void app_output_pattern_init(app_output_pattern_t *p, const char *name,
                             app_status_t (*write)(uint16_t), void (*done)(void))
{
    p->write = write;
    p->done = done;
    p->stopping = 0;
    p->remaining = 0;
    rt_timer_init(&p->timer, name, next_phase, p, 1,
                  RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_HARD_TIMER);
}

app_status_t app_output_pattern_set(app_output_pattern_t *p, uint16_t level)
{
    rt_base_t irq = rt_hw_interrupt_disable();
    app_status_t rc;
    if (!level) rc = stop_output(p, 0);
    else if (p->stopping) rc = APP_ERR_BUSY;
    else {
        rc = p->write(level);
        if (rc == APP_OK) finish(p);
        else (void)stop_output(p, 0);
    }
    rt_hw_interrupt_enable(irq);
    return rc;
}

app_status_t app_output_pattern_start(app_output_pattern_t *p, app_output_phase_t first,
                                      app_output_phase_t second, uint32_t count)
{
    rt_tick_t first_delay = first.ticks + 1U;
    rt_base_t irq = rt_hw_interrupt_disable();
    p->phase[0] = first;
    p->phase[1] = second;
    p->index = 0;
    p->remaining = count;
    app_status_t rc = APP_ERR_IO;
    if (rt_timer_control(&p->timer, RT_TIMER_CTRL_SET_TIME, &first_delay) == RT_EOK &&
        rt_timer_start(&p->timer) == RT_EOK) rc = p->write(first.level);
    if (rc != APP_OK) (void)stop_output(p, 0);
    rt_hw_interrupt_enable(irq);
    return rc;
}

void app_output_pattern_release(app_output_pattern_t *p)
{
    rt_base_t irq = rt_hw_interrupt_disable();
    finish(p);
    rt_timer_detach(&p->timer);
    rt_hw_interrupt_enable(irq);
}
