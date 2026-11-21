#include "dma.h"
#include <rthw.h>

void dma_init(DMA_Channel_TypeDef *channel)
{
    uint32_t clock = (uintptr_t)channel < DMA2_BASE ? RCC_AHBPeriph_DMA1 : RCC_AHBPeriph_DMA2;
    rt_base_t irq = rt_hw_interrupt_disable();
    RCC_AHBPeriphClockCmd(clock, ENABLE);
    rt_hw_interrupt_enable(irq);
    DMA_DeInit(channel);
}

void dma_start(DMA_Channel_TypeDef *channel)
{
    __sync_synchronize();
    DMA_Cmd(channel, ENABLE);
}

void dma_stop(DMA_Channel_TypeDef *channel)
{
    DMA_Cmd(channel, DISABLE);
    __sync_synchronize();
}
