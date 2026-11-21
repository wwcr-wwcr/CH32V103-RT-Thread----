#include "bsp_rng.h"
#include "bsp_device_internal.h"
#include "ch32v30x_rng.h"
#define BSP_RNG_POLLS 100000UL
static bsp_device_t rng;

bsp_status_t bsp_rng_init(void)
{
    bsp_status_t result = bsp_device_enter(&rng, 0);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    if (RCC->AHBPCENR & RCC_AHBPeriph_RNG) result = BSP_ERR_BUSY;
    else {
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, ENABLE);
        RNG_ClearFlag(RNG_FLAG_CECS | RNG_FLAG_SECS);
        RNG_Cmd(ENABLE);
    }
    bsp_irq_restore(level);
    bsp_device_leave(&rng, result == BSP_OK);
    return result;
}

bsp_status_t bsp_rng_read(uint32_t *dst, size_t count, size_t *written)
{
    if (written) *written = 0;
    if (!dst || !count) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&rng, 1);
    if (result != BSP_OK) return result;
    size_t i = 0;
    for (; i < count; ++i) {
        unsigned polls = BSP_RNG_POLLS;
        unsigned flags;
        do {
            flags = RNG->SR;
        } while (!(flags & (RNG_FLAG_DRDY | RNG_FLAG_CECS | RNG_FLAG_SECS)) && --polls);
        if (flags & (RNG_FLAG_CECS | RNG_FLAG_SECS)) { result = BSP_ERR_IO; break; }
        if (!(flags & RNG_FLAG_DRDY)) { result = BSP_ERR_TIMEOUT; break; }
        dst[i] = RNG_GetRandomNumber();
    }
    if (written) *written = i;
    bsp_device_leave(&rng, 1);
    return result;
}

bsp_status_t bsp_rng_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&rng, 1);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    RNG_Cmd(DISABLE);
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_RNG, DISABLE);
    bsp_irq_restore(level);
    bsp_device_leave(&rng, 0);
    return BSP_OK;
}
