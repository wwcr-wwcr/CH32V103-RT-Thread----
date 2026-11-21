#include "gpio.h"
#include <rthw.h>

void gpio_config(GPIO_TypeDef *port, uint16_t pins, GPIOMode_TypeDef mode)
{
    uint32_t clock;
    if (port == GPIOA) clock = RCC_APB2Periph_GPIOA;
    else if (port == GPIOB) clock = RCC_APB2Periph_GPIOB;
    else if (port == GPIOC) clock = RCC_APB2Periph_GPIOC;
    else if (port == GPIOD) clock = RCC_APB2Periph_GPIOD;
    else if (port == GPIOE) clock = RCC_APB2Periph_GPIOE;
    else return;

    GPIO_InitTypeDef config = {pins, GPIO_Speed_50MHz, mode};
    rt_base_t irq = rt_hw_interrupt_disable();
    RCC_APB2PeriphClockCmd(clock, ENABLE);
    GPIO_Init(port, &config);
    rt_hw_interrupt_enable(irq);
}
