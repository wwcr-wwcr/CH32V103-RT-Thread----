#include "bsp_dma.h"
#include "bsp_adc_timing_internal.h"
#include "bsp_dac_playback_internal.h"
#include "bsp_ahb_internal.h"
#include "bsp_board.h"

static DMA_Channel_TypeDef *const channels[] = {
    DMA1_Channel1, DMA1_Channel2, DMA1_Channel3, DMA1_Channel4, DMA1_Channel5,
    DMA1_Channel6, DMA1_Channel7, DMA2_Channel1, DMA2_Channel2, DMA2_Channel3,
    DMA2_Channel4, DMA2_Channel5, DMA2_Channel6, DMA2_Channel7, DMA2_Channel8,
    DMA2_Channel9, DMA2_Channel10, DMA2_Channel11
};
static struct { uint8_t active; volatile uint8_t lock; } slots[sizeof(channels) / sizeof(channels[0])];
static struct { uint8_t count, already_enabled; } controllers[2];

static void clock_get(unsigned controller)
{
    uint32_t gate = controller ? RCC_AHBPeriph_DMA2 : RCC_AHBPeriph_DMA1;
    long level = bsp_irq_save();
    if (!controllers[controller].count++) {
        controllers[controller].already_enabled = (RCC->AHBPCENR & gate) != 0;
        RCC_AHBPeriphClockCmd(gate, ENABLE);
    }
    bsp_irq_restore(level);
}

static void clock_put(unsigned controller)
{
    uint32_t gate = controller ? RCC_AHBPeriph_DMA2 : RCC_AHBPeriph_DMA1;
    long level = bsp_irq_save();
    if (!--controllers[controller].count && !controllers[controller].already_enabled &&
        !(controller && BSP_BOARD_LCD_ACTIVE)) {
        /* 保护也可能由其他底层驱动直接启用的通道。 */
        int in_use = 0;
        for (unsigned i = controller ? 7 : 0; i < (controller ? 18u : 7u); ++i)
            in_use |= (channels[i]->CFGR & DMA_CFGR1_EN) != 0;
        if (!in_use) RCC_AHBPeriphClockCmd(gate, DISABLE);
    }
    bsp_irq_restore(level);
}

static int index_of(DMA_Channel_TypeDef *channel)
{
    size_t i;
    for (i = 0; i < sizeof(channels) / sizeof(channels[0]); ++i)
        if (channel == channels[i]) return (int)i;
    return -1;
}

static bsp_status_t claim_channel(int i)
{
    DMA_Channel_TypeDef *channel = channels[i];
    bsp_status_t status = BSP_ERR_BUSY;
    if (!slots[i].active) {
        clock_get(i >= 7);
        if (!(channel->CFGR & DMA_CFGR1_EN)) {
            DMA_DeInit(channel);
            slots[i].active = 1;
            status = BSP_OK;
        } else clock_put(i >= 7);
    }
    return status;
}

static void release_channel(int i)
{
    DMA_DeInit(channels[i]);
    slots[i].active = 0;
    clock_put(i >= 7);
}

/* DMA2 channel 4 is slot 10. Hold its normal BSP lock for all of playback. */
bsp_status_t bsp_dac_hold_dma(void)
{
    if (bsp_try_lock(&slots[10].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = claim_channel(10);
    if (rc != BSP_OK) bsp_unlock(&slots[10].lock);
    return rc;
}

void bsp_dac_release_dma(void)
{
    release_channel(10);
    bsp_unlock(&slots[10].lock);
}

bsp_status_t bsp_dma_init(DMA_Channel_TypeDef *channel)
{
    int i = index_of(channel);
    if (i < 0) return BSP_ERR_ARG;
    if (BSP_BOARD_LCD_ACTIVE && channel == DMA2_Channel2) return BSP_ERR_BUSY;
    if (bsp_try_lock(&slots[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = claim_channel(i);
    bsp_unlock(&slots[i].lock);
    return status;
}

static bsp_status_t run_transfer(int i, bsp_dma_direction_t dir, uintptr_t source, uintptr_t dest,
                                 uint16_t count, uint8_t width, uint32_t poll_limit,
                                 void (*start)(uint32_t), uint32_t start_arg)
{
    DMA_Channel_TypeDef *channel = channels[i];
    DMA_InitTypeDef init = {0};
    DMA_TypeDef *bank = i < 7 ? DMA1 : i < 14 ? DMA2 : DMA2_EXTEN;
    uint32_t shift = (i % 7) * 4;
    init.DMA_PeripheralBaseAddr = dir == BSP_DMA_MEM_TO_PERIPH ? dest : source;
    init.DMA_MemoryBaseAddr = dir == BSP_DMA_MEM_TO_PERIPH ? source : dest;
    init.DMA_BufferSize = count;
    init.DMA_DIR = dir == BSP_DMA_MEM_TO_PERIPH ? DMA_DIR_PeripheralDST : DMA_DIR_PeripheralSRC;
    init.DMA_PeripheralInc = dir == BSP_DMA_MEM_TO_MEM ? DMA_PeripheralInc_Enable : DMA_PeripheralInc_Disable;
    init.DMA_MemoryInc = DMA_MemoryInc_Enable;
    init.DMA_PeripheralDataSize = width == 4 ? DMA_PeripheralDataSize_Word :
                                  width == 2 ? DMA_PeripheralDataSize_HalfWord : DMA_PeripheralDataSize_Byte;
    init.DMA_MemoryDataSize = width == 4 ? DMA_MemoryDataSize_Word :
                              width == 2 ? DMA_MemoryDataSize_HalfWord : DMA_MemoryDataSize_Byte;
    init.DMA_M2M = dir == BSP_DMA_MEM_TO_MEM ? DMA_M2M_Enable : DMA_M2M_Disable;
    /* 带 TIM4 启动回调的是定频 ADC；其读 RDATAR 的请求优先于普通长块复制。 */
    init.DMA_Priority = start ? DMA_Priority_VeryHigh : DMA_Priority_Low;
    DMA_Init(channel, &init);
    bank->INTFCR = 1u << shift;
    __asm volatile ("fence iorw, iorw" ::: "memory");
    DMA_Cmd(channel, ENABLE);
    if (start) start(start_arg);
    bsp_status_t status = BSP_ERR_TIMEOUT;
    while (poll_limit--) {
        uint32_t flags = bank->INTFR >> shift;
        if (flags & 8u) { status = BSP_ERR_IO; break; }
        if (flags & 2u) { status = BSP_OK; break; }
        if (start && TIM8->CNT == count) {
            /* 硬件帧门已关闭，最多还可能有最后一笔请求在途。 */
            if (channel->CNTR > 1U) { status = BSP_ERR_IO; break; }
            if (poll_limit > 1024U) poll_limit = 1024U;
        }
    }
    DMA_Cmd(channel, DISABLE);
    bank->INTFCR = 1u << shift;
    __asm volatile ("fence iorw, iorw" ::: "memory");
    return status;
}

bsp_status_t bsp_dma_transfer(DMA_Channel_TypeDef *channel, bsp_dma_direction_t dir,
                              const volatile void *src, volatile void *dst,
                              uint16_t count, uint8_t width, uint32_t poll_limit)
{
    int i = index_of(channel);
    uintptr_t source = (uintptr_t)src, dest = (uintptr_t)dst;
    if (i < 0 || (unsigned)dir > BSP_DMA_PERIPH_TO_MEM || !src || !dst || !count || !poll_limit ||
        (width != 1 && width != 2 && width != 4) || ((source | dest) & (width - 1))) return BSP_ERR_ARG;
    if (dir == BSP_DMA_MEM_TO_MEM &&
        (source >= dest ? source - dest : dest - source) < (size_t)count * width) return BSP_ERR_ARG;
    if (bsp_try_lock(&slots[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = slots[i].active
        ? run_transfer(i, dir, source, dest, count, width, poll_limit, NULL, 0) : BSP_ERR_STATE;
    bsp_unlock(&slots[i].lock);
    return status;
}

bsp_status_t bsp_dma_de_init(DMA_Channel_TypeDef *channel)
{
    int i = index_of(channel);
    if (i < 0) return BSP_ERR_ARG;
    if (bsp_try_lock(&slots[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (slots[i].active) {
        release_channel(i);
        status = BSP_OK;
    }
    bsp_unlock(&slots[i].lock);
    return status;
}

/* TIM4_UP -> DMA1_CH7。持锁覆盖申请、配置、传输和释放，外部线程无法中途解初始化。 */
bsp_status_t bsp_adc_frame_dma(ADC_TypeDef *adc, uint16_t *dst, uint16_t count,
                               uint32_t poll_limit, void (*start)(uint32_t), uint32_t frame_ms)
{
    const int i = 6;
    if (bsp_try_lock(&slots[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = claim_channel(i);
    if (rc == BSP_OK) {
        rc = run_transfer(i, BSP_DMA_PERIPH_TO_MEM, (uintptr_t)&adc->RDATAR,
                          (uintptr_t)dst, count, 2, poll_limit, start, frame_ms);
        /* CC4 仍可触发尾部 EOC 检查；先关闭 UDE 再交还通道，避免触发下一个 DMA 用户。 */
        TIM_DMACmd(TIM4, TIM_DMA_Update, DISABLE);
        release_channel(i);
    }
    bsp_unlock(&slots[i].lock);
    return rc;
}
