#include "bsp_wwdg.h"
#include "bsp_device_internal.h"

static bsp_device_t wwdg;
static uint8_t counter, threshold, started;

bsp_status_t bsp_wwdg_init(uint32_t prescaler, uint8_t window, uint8_t reload)
{
    if ((prescaler & ~WWDG_Prescaler_8) || window <= 0x40 || reload > 0x7F || window > reload)
        return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&wwdg, 0);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    if (RCC->APB1PCENR & RCC_APB1Periph_WWDG) result = BSP_ERR_BUSY;
    else {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, ENABLE);
        WWDG_SetPrescaler(prescaler);
        WWDG_SetWindowValue(window);
        counter = reload;
        threshold = window;
        started = 0;
    }
    bsp_irq_restore(level);
    bsp_device_leave(&wwdg, result == BSP_OK);
    return result;
}

bsp_status_t bsp_wwdg_start(void)
{
    bsp_status_t result = bsp_device_enter(&wwdg, 1);
    if (result != BSP_OK) return result;
    if (started) result = BSP_ERR_BUSY;
    else { WWDG_Enable(counter); started = 1; }
    bsp_device_leave(&wwdg, 1);
    return result;
}

bsp_status_t bsp_wwdg_feed(void)
{
    bsp_status_t result = bsp_device_enter(&wwdg, 1);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    if (!started) result = BSP_ERR_STATE;
    else if ((WWDG->CTLR & 0x7FU) >= threshold) result = BSP_ERR_BUSY;
    else WWDG_SetCounter(counter);
    bsp_irq_restore(level);
    bsp_device_leave(&wwdg, 1);
    return result;
}

bsp_status_t bsp_wwdg_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&wwdg, 1);
    if (result != BSP_OK) return result;
    if (started) result = BSP_ERR_UNSUPPORTED;
    else {
        long level = bsp_irq_save();
        WWDG_DeInit();
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_WWDG, DISABLE);
        bsp_irq_restore(level);
    }
    bsp_device_leave(&wwdg, result != BSP_OK);
    return result;
}
