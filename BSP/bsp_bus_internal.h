#ifndef BSP_BUS_INTERNAL_H
#define BSP_BUS_INTERNAL_H

#include "bsp_resource.h"

/* 单次事务累计可空转的轮询次数。无需 SysTick，在调度器启动前亦有界。 */
#ifndef BSP_BUS_POLL_LIMIT
#define BSP_BUS_POLL_LIMIT 1000000UL
#endif
#if BSP_BUS_POLL_LIMIT < 1 || BSP_BUS_POLL_LIMIT > UINT32_MAX
#error BSP_BUS_POLL_LIMIT must fit in a nonzero uint32_t
#endif

/* 调用者应在很短的 bsp_irq_save/restore 内完成共享 RCC/AFIO 的读改写。 */
static inline uint32_t bus_set_route(volatile uint32_t *reg, uint32_t mask, uint32_t value)
{
    uint32_t old = *reg;
    *reg = (old & ~mask) | (value & mask);
    return old & mask;
}

static inline void bus_release_pins(const void *owner, const bsp_pin_t *pins, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        bsp_pin_floating(pins[i]);
    bsp_pins_release(owner, pins, count);
}
#endif
