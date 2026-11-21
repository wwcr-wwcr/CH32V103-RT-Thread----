#include "bsp_dac.h"
#include "bsp_dac_playback_internal.h"
#include "bsp_resource.h"

static uint8_t owner, looping;

bsp_status_t bsp_dac_playback_start(uint8_t channel, const uint16_t *table, uint16_t count,
                                     uint32_t sample_hz, uint8_t circular, float *actual_hz)
{
    uintptr_t addr = (uintptr_t)table;
    if ((channel != 1 && channel != 2) || !count || !sample_hz || sample_hz > 100000U ||
        !actual_hz || circular > 1 || (addr & 1U) || addr < SRAM_BASE ||
        addr >= SRAM_BASE + 65536U || count > (SRAM_BASE + 65536U - addr) / 2U) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t rc = owner ? BSP_ERR_BUSY : bsp_dac_hold_dma();
    float actual = 0;
    if (rc == BSP_OK) {
        rc = bsp_dac_hold_timer(sample_hz, &actual);
        if (rc != BSP_OK) bsp_dac_release_dma();
        else {
            rc = bsp_dac_hold_output(channel);
            if (rc != BSP_OK) { bsp_dac_release_timer(); bsp_dac_release_dma(); }
        }
    }
    if (rc == BSP_OK) {
        DMA_InitTypeDef dma = {0};
        dma.DMA_MemoryBaseAddr = (uint32_t)addr;
        dma.DMA_PeripheralBaseAddr = (uint32_t)(uintptr_t)(channel == 1 ? &DAC->R12BDHR1 : &DAC->R12BDHR2);
        dma.DMA_BufferSize = count;
        dma.DMA_DIR = DMA_DIR_PeripheralDST;
        dma.DMA_MemoryInc = DMA_MemoryInc_Enable;
        dma.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
        dma.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
        dma.DMA_Mode = circular ? DMA_Mode_Circular : DMA_Mode_Normal;
        dma.DMA_Priority = DMA_Priority_High;
        DMA_Init(DMA2_Channel4, &dma);
        DMA2->INTFCR = 0xF000U;
        __asm volatile ("fence iorw, iorw" ::: "memory");
        DMA_Cmd(DMA2_Channel4, ENABLE);
        TIM_DMACmd(TIM7, TIM_DMA_Update, ENABLE);
        TIM_Cmd(TIM7, ENABLE);
        owner = channel;
        looping = circular;
        *actual_hz = actual;
    }
    bsp_irq_restore(level);
    return rc;
}

bsp_status_t bsp_dac_playback_running(uint8_t channel, uint8_t *running)
{
    if (!running || (channel != 1 && channel != 2)) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t rc = BSP_ERR_STATE;
    if (channel == owner) {
        uint32_t flags = DMA2->INTFR >> 12;
        rc = flags & 8U ? BSP_ERR_IO : BSP_OK;
        if (rc == BSP_OK) *running = looping || !(flags & 2U);
    }
    bsp_irq_restore(level);
    return rc;
}

bsp_status_t bsp_dac_playback_stop(uint8_t channel, uint16_t idle)
{
    if ((channel != 1 && channel != 2) || idle > 4095U) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t rc = BSP_ERR_STATE;
    if (channel == owner) {
        TIM_Cmd(TIM7, DISABLE);
        TIM_DMACmd(TIM7, TIM_DMA_Update, DISABLE);
        DMA_Cmd(DMA2_Channel4, DISABLE);
        __asm volatile ("fence iorw, iorw" ::: "memory");
        bsp_dac_release_dma();
        bsp_dac_release_timer();
        bsp_dac_release_output(channel, idle);
        owner = 0;
        rc = BSP_OK;
    }
    bsp_irq_restore(level);
    return rc;
}
