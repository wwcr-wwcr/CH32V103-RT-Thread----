#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#include "app_types.h"
#include "bsp.h"
#include <rtthread.h>
#include <rthw.h>

typedef struct { volatile uint8_t lock; uint8_t active; } app_device_t;
static inline int app_can_sleep(void)
{
#ifdef __riscv
    if (!(__get_MSTATUS() & 8U)) return 0; /* mstatus.MIE，线程不可在关中断时等待。 */
#endif
    return rt_thread_self() != RT_NULL && !rt_interrupt_get_nest() && !rt_critical_level();
}
static inline app_status_t app_from_bsp(bsp_status_t status)
{
    return (app_status_t)status;
}

/* 非阻塞线程级设备锁；长传输与延时期间保持中断开启。 */
static inline app_status_t app_try_lock(volatile uint8_t *lock)
{
    rt_base_t level = rt_hw_interrupt_disable();
    app_status_t result = *lock ? APP_ERR_BUSY : APP_OK;
    *lock = 1;
    rt_hw_interrupt_enable(level);
    return result;
}

static inline void app_unlock(volatile uint8_t *lock)
{
    rt_base_t level = rt_hw_interrupt_disable();
    *lock = 0;
    rt_hw_interrupt_enable(level);
}

static inline app_status_t app_device_enter(app_device_t *dev, int need_active)
{
    app_status_t result = app_try_lock(&dev->lock);
    if (result != APP_OK) return result;
    if (dev->active == need_active) return APP_OK;
    app_unlock(&dev->lock);
    return need_active ? APP_ERR_STATE : APP_ERR_BUSY;
}
static inline void app_device_leave(app_device_t *dev, int active)
{
    dev->active = (uint8_t)active;
    app_unlock(&dev->lock);
}
#endif
