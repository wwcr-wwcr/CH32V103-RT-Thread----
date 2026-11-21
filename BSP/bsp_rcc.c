#include "bsp_rcc.h"
#include "bsp_device_internal.h"

static bsp_device_t mco;
static const bsp_pin_t pin = {GPIOA, GPIO_Pin_8};

bsp_status_t bsp_rcc_read(RCC_ClocksTypeDef *frequencies)
{
    if (!frequencies) return BSP_ERR_ARG;
    long key = bsp_irq_save();
    RCC_GetClocksFreq(frequencies);
    bsp_irq_restore(key);
    return BSP_OK;
}

bsp_status_t bsp_rcc_mco_init(uint8_t source)
{
    if (source < RCC_MCO_SYSCLK || source > RCC_MCO_PLLCLK_Div2) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&mco, 0);
    if (result != BSP_OK) return result;
    long key = bsp_irq_save();
    uint32_t ready = source == RCC_MCO_HSI ? RCC_HSIRDY :
                     source == RCC_MCO_HSE ? RCC_HSERDY :
                     source == RCC_MCO_PLLCLK_Div2 ? RCC_PLLRDY : 0;
    if (ready && !(RCC->CTLR & ready)) result = BSP_ERR_STATE;
    else if (RCC->CFGR0 & RCC_CFGR0_MCO) result = BSP_ERR_BUSY;
    else result = bsp_pins_claim(&mco, &pin, 1);
    if (result == BSP_OK) {
        bsp_pin_config(pin, GPIO_Mode_AF_PP);
        /* CFGR0 高字节还包含 ETHPRE 和 ADCDUTY，仅更新 MCO 字段。 */
        RCC->CFGR0 = (RCC->CFGR0 & ~RCC_CFGR0_MCO) | ((uint32_t)source << 24);
    }
    bsp_irq_restore(key);
    bsp_device_leave(&mco, result == BSP_OK);
    return result;
}

bsp_status_t bsp_rcc_mco_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&mco, 1);
    if (result != BSP_OK) return result;
    long key = bsp_irq_save();
    RCC->CFGR0 &= ~RCC_CFGR0_MCO;
    bsp_pin_floating(pin);
    bsp_pins_release(&mco, &pin, 1);
    bsp_irq_restore(key);
    bsp_device_leave(&mco, 0);
    return BSP_OK;
}
