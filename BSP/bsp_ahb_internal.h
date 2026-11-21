#ifndef BSP_AHB_INTERNAL_H
#define BSP_AHB_INTERNAL_H

#include "bsp_resource.h"

/* AHB 时钟使能寄存器由多个驱动共用，仅在读改写期间关中断。 */
static inline void bsp_ahb_clock(uint32_t mask, FunctionalState state)
{
    long level = bsp_irq_save();
    RCC_AHBPeriphClockCmd(mask, state);
    bsp_irq_restore(level);
}

static inline void bsp_ahb_release_pins(const void *owner, const bsp_pin_t *pins, size_t count)
{
    size_t i;
    for (i = 0; i < count; ++i)
        bsp_pin_floating(pins[i]);
    bsp_pins_release(owner, pins, count);
}

#endif
