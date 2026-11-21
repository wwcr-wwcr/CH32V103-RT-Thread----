#include "bsp_iwdg.h"
#include "bsp_device_internal.h"

#define BSP_IWDG_POLLS 1000000UL
static bsp_device_t iwdg;
static uint8_t started, needs_restore, lsi_was_on, old_prescaler;
static uint16_t old_reload;

static bsp_status_t wait_update(void)
{
    for (uint32_t n = BSP_IWDG_POLLS; n; --n)
        if (!(IWDG->STATR & (IWDG_FLAG_PVU | IWDG_FLAG_RVU))) return BSP_OK;
    return BSP_ERR_TIMEOUT;
}

static bsp_status_t set_config(uint8_t prescaler, uint16_t reload)
{
    bsp_status_t result = wait_update();
    if (result == BSP_OK) {
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Enable);
        IWDG_SetPrescaler(prescaler);
        IWDG_SetReload(reload);
        result = wait_update();
        IWDG_WriteAccessCmd(IWDG_WriteAccess_Disable);
    }
    return result;
}

static void release_lsi(void)
{
    long key = bsp_irq_save();
    if (!lsi_was_on && (RCC->BDCTLR & RCC_RTCSEL) != RCC_RTCCLKSource_LSI)
        RCC_LSICmd(DISABLE);
    bsp_irq_restore(key);
}

bsp_status_t bsp_iwdg_init(uint8_t prescaler, uint16_t reload)
{
    if (prescaler > IWDG_Prescaler_256 || reload > 4095) return BSP_ERR_ARG;
    if (!(FLASH->OBR & FLASH_OBR_WDG_SW)) return BSP_ERR_UNSUPPORTED;
    bsp_status_t result = bsp_device_enter(&iwdg, 0);
    if (result != BSP_OK) return result;
    long key = bsp_irq_save();
    lsi_was_on = (RCC->RSTSCKR & RCC_LSION) != 0;
    RCC_LSICmd(ENABLE);
    bsp_irq_restore(key);
    uint32_t n = BSP_IWDG_POLLS;
    while (n && !(RCC->RSTSCKR & RCC_LSIRDY)) --n;
    result = n ? wait_update() : BSP_ERR_TIMEOUT;
    needs_restore = 0;
    if (result == BSP_OK) {
        old_prescaler = IWDG->PSCR;
        old_reload = IWDG->RLDR;
        result = set_config(prescaler, reload);
        if (result != BSP_OK) needs_restore = 1;
    }
    if (result != BSP_OK && !needs_restore) release_lsi();
    started = 0;
    bsp_device_leave(&iwdg, result == BSP_OK || needs_restore);
    return result;
}

bsp_status_t bsp_iwdg_start(void)
{
    bsp_status_t result = bsp_device_enter(&iwdg, 1);
    if (result != BSP_OK) return result;
    if (needs_restore) result = BSP_ERR_STATE;
    else if (started) result = BSP_ERR_BUSY;
    else {
        IWDG_ReloadCounter();
        IWDG->CTLR = 0xCCCCU; /* 硬件启动键。LSIRDY 已在 init 中限时确认。 */
        started = 1;
    }
    bsp_device_leave(&iwdg, 1);
    return result;
}

bsp_status_t bsp_iwdg_feed(void)
{
    bsp_status_t result = bsp_device_enter(&iwdg, 1);
    if (result != BSP_OK) return result;
    if (started) IWDG_ReloadCounter();
    else result = BSP_ERR_STATE;
    bsp_device_leave(&iwdg, 1);
    return result;
}

bsp_status_t bsp_iwdg_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&iwdg, 1);
    if (result != BSP_OK) return result;
    result = started ? BSP_ERR_UNSUPPORTED : set_config(old_prescaler, old_reload);
    if (result == BSP_OK) { needs_restore = 0; release_lsi(); }
    bsp_device_leave(&iwdg, result != BSP_OK);
    return result;
}
