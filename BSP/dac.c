#include "dac.h"
#include "dma.h"
#include "gpio.h"
#include "tim.h"
#include <rtthread.h>
#include <rthw.h>

typedef struct {
    TIM_TypeDef *timer;
    DMA_Channel_TypeDef *dma;
    uint32_t channel;
    uint16_t pin, code;
    uint8_t ready, playing;
} dac_port_t;

static dac_port_t pa4 = {TIM6, DMA2_Channel3, DAC_Channel_1, GPIO_Pin_4};
static dac_port_t pa5 = {TIM7, DMA2_Channel4, DAC_Channel_2, GPIO_Pin_5};
static struct rt_mutex mutex;
static int dac_ipc_init(void) { return rt_mutex_init(&mutex, "dac2", RT_IPC_FLAG_PRIO); }
INIT_COMPONENT_EXPORT(dac_ipc_init);

static void prepare(dac_port_t *port)
{
    if (port->playing) {
        TIM_Cmd(port->timer, DISABLE);
        TIM_DMACmd(port->timer, TIM_DMA_Update, DISABLE);
        dma_stop(port->dma);
        port->playing = 0;
    }
    if (!port->ready) {
        gpio_config(GPIOA, port->pin, GPIO_Mode_AIN);
        DAC_InitTypeDef config;
        DAC_StructInit(&config);
        rt_base_t irq = rt_hw_interrupt_disable();
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
        DAC_Init(port->channel, &config);
        DAC_Cmd(port->channel, ENABLE);
        rt_hw_interrupt_enable(irq);
        port->ready = 1;
    }
}

/* 单点用软件请求传输一个半字，波形由 TIM6/TIM7 更新事件逐点请求 DMA。 */
static void configure_dma(dac_port_t *port, const uint16_t *data, uint16_t count, int circular)
{
    DMA_InitTypeDef cfg = {0};
    cfg.DMA_PeripheralBaseAddr = (uintptr_t)(port == &pa4 ? &DAC->R12BDHR1 : &DAC->R12BDHR2);
    cfg.DMA_MemoryBaseAddr = (uintptr_t)data;
    cfg.DMA_DIR = DMA_DIR_PeripheralDST;
    cfg.DMA_BufferSize = count;
    cfg.DMA_MemoryInc = circular ? DMA_MemoryInc_Enable : DMA_MemoryInc_Disable;
    cfg.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    cfg.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    cfg.DMA_Priority = DMA_Priority_High;
    cfg.DMA_Mode = circular ? DMA_Mode_Circular : DMA_Mode_Normal;
    cfg.DMA_M2M = circular ? DMA_M2M_Disable : DMA_M2M_Enable;
    dma_init(port->dma);
    DMA_Init(port->dma, &cfg);
    dma_start(port->dma);
}

static int write_code(dac_port_t *port, uint16_t code)
{
    prepare(port);
    port->code = code;
    /* 先清掉上一次传输留下的 TC/TE 标志：否则下面的轮询会立刻“成功”，
     * dma_stop() 在新传输落地前就把它掐断 —— 表现为上电后第一次写正常、
     * 之后每次 dac_write 都返回 0 但输出不再变化。 */
    DMA_ClearFlag(port == &pa4 ? DMA2_FLAG_GL3 : DMA2_FLAG_GL4);
    configure_dma(port, &port->code, 1, 0);
    uint32_t done = port == &pa4 ? DMA2_FLAG_TC3 : DMA2_FLAG_TC4;
    uint32_t error = port == &pa4 ? DMA2_FLAG_TE3 : DMA2_FLAG_TE4;
    int rc = -RT_ETIMEOUT;
    for (unsigned i = 0; i < 1000; ++i) {
        if (DMA_GetFlagStatus(error)) { rc = -RT_EIO; break; }
        if (DMA_GetFlagStatus(done)) { rc = RT_EOK; break; }
    }
    dma_stop(port->dma);
    return rc;
}

static int play(dac_port_t *port, const uint16_t *wave, size_t count, uint32_t rate_hz)
{
    extern const char _eusrstack[];
    uintptr_t addr = (uintptr_t)wave, end = (uintptr_t)_eusrstack;
    if (!count || count > UINT16_MAX || !rate_hz || rate_hz > 1000000U ||
        (addr & 1U) || addr < SRAM_BASE || addr >= end || count > (end - addr) / 2U) return -RT_EINVAL;
    prepare(port);
    int rc = tim_prepare(port->timer, rate_hz);
    if (rc != RT_EOK) return rc;
    configure_dma(port, wave, count, 1);
    TIM_DMACmd(port->timer, TIM_DMA_Update, ENABLE);
    port->playing = 1;
    TIM_Cmd(port->timer, ENABLE);
    return RT_EOK;
}

int dac1_write(uint16_t code) { return code <= 4095 ? write_code(&pa4, code) : -RT_EINVAL; }

int dac1_play(const uint16_t *wave, uint16_t count, uint32_t rate_hz)
{
    return play(&pa4, wave, count, rate_hz);
}

int dac_write(uint16_t code)
{
    if (code > 4095) return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    rc = write_code(&pa5, code);
    rt_mutex_release(&mutex);
    return rc;
}

int dac_stop(void) { return dac_write(0); }

int dac_play(const uint16_t *wave, size_t count, uint32_t rate_hz)
{
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    rc = play(&pa5, wave, count, rate_hz);
    rt_mutex_release(&mutex);
    return rc;
}
