#include "bsp_dbgmcu.h"
#include "bsp_device_internal.h"
static bsp_device_t debug;
static uint32_t added;
bsp_status_t bsp_dbgmcu_init(uint32_t mask)
{
    if (!mask || (mask & ~0x00FFFF07UL)) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&debug, 0);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    uint32_t old = __get_DEBUG_CR();
    added = mask & ~old;
    __set_DEBUG_CR(old | mask);
    bsp_irq_restore(level);
    bsp_device_leave(&debug, 1);
    return BSP_OK;
}

uint32_t bsp_dbgmcu_chip_id(void) { return DBGMCU_GetCHIPID(); }

bsp_status_t bsp_dbgmcu_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&debug, 1);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    __set_DEBUG_CR(__get_DEBUG_CR() & ~added);
    bsp_irq_restore(level);
    bsp_device_leave(&debug, 0);
    return BSP_OK;
}
