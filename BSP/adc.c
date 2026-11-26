#include "adc.h"
#include "dma.h"
#include "gpio.h"
#include "tim.h"
#include "ch32v30x_it.h"
#include <rtthread.h>
#include <rthw.h>

static struct rt_mutex mutex;
static struct rt_semaphore complete;
static volatile int dma_result;
static uint32_t adc_clock;
static int timed;

static int adc_ipc_init(void)
{
    int rc = rt_mutex_init(&mutex, "adc1", RT_IPC_FLAG_PRIO);
    return rc == RT_EOK ? rt_sem_init(&complete, "adc_done", 0, RT_IPC_FLAG_FIFO) : rc;
}
INIT_COMPONENT_EXPORT(adc_ipc_init);

static uint32_t select_clock(uint32_t *prescaler)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    static const uint32_t values[] = {RCC_PCLK2_Div2, RCC_PCLK2_Div4, RCC_PCLK2_Div6, RCC_PCLK2_Div8};
    for (unsigned i = 0; i < 4; ++i) {
        unsigned divisor = 2U * i + 2U;
        if (clocks.PCLK2_Frequency <= 14000000U * divisor) {
            *prescaler = values[i];
            return clocks.PCLK2_Frequency / divisor;
        }
    }
    return 0;
}

uint32_t adc_max_rate_hz(void)
{
    uint32_t prescaler;
    return select_clock(&prescaler) / 14U;
}

static int prepare(void)
{
    if (adc_clock) return RT_EOK;
    uint32_t prescaler;
    uint32_t hz = select_clock(&prescaler);
    if (!hz) return -RT_EINVAL;
    gpio_config(GPIOB, GPIO_Pin_1, GPIO_Mode_AIN);
    gpio_config(GPIOA, GPIO_Pin_1, GPIO_Mode_AIN);
    rt_base_t irq = rt_hw_interrupt_disable();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1, ENABLE);
    RCC_ADCCLKConfig(prescaler);
    ADC_DeInit(ADC1);
    rt_hw_interrupt_enable(irq);
    ADC_InitTypeDef config = {0};
    ADC_StructInit(&config);
    config.ADC_OutputBuffer = ADC_OutputBuffer_Disable;
    config.ADC_Pga = ADC_Pga_1;
    config.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_Init(ADC1, &config);
    ADC_Cmd(ADC1, ENABLE);
    ADC_BufferCmd(ADC1, DISABLE);
    rt_hw_us_delay(2);
    ADC_ResetCalibration(ADC1);
    unsigned limit = 100000;
    while ((ADC1->CTLR2 & ADC_RSTCAL) && --limit) {}
    if (!limit) return -RT_ETIMEOUT;
    ADC_StartCalibration(ADC1);
    limit = 100000;
    while ((ADC1->CTLR2 & ADC_CAL) && --limit) {}
    if (!limit) return -RT_ETIMEOUT;
    ADC_BufferCmd(ADC1, ENABLE);
    adc_clock = hz;
    return RT_EOK;
}

static void stop_sampling(void)
{
    if (timed) TIM_Cmd(TIM3, DISABLE);
    ADC_ExternalTrigConvCmd(ADC1, DISABLE);
    ADC1->CTLR2 &= ~ADC_CONT;
    ADC_DMACmd(ADC1, DISABLE);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC | DMA_IT_TE, DISABLE);
    dma_stop(DMA1_Channel1);
}

void DMA1_Channel1_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void DMA1_Channel1_IRQHandler(void)
{
    GET_INT_SP();
    rt_interrupt_enter();
    if (DMA_GetFlagStatus(DMA1_FLAG_TC1) || DMA_GetFlagStatus(DMA1_FLAG_TE1)) {
        dma_result = DMA_GetFlagStatus(DMA1_FLAG_TE1) ? -RT_EIO : RT_EOK;
        stop_sampling();
        DMA_ClearFlag(DMA1_FLAG_GL1);
        rt_sem_release(&complete);
    }
    rt_interrupt_leave();
    FREE_INT_SP();
}

static int transfer(adc_input_t input, uint16_t *samples, size_t count, uint32_t rate)
{
    /* 含采样窗口的总转换周期。定时时选择能容纳的最长窗口。 */
    static const unsigned cycles[] = {14, 20, 26, 41, 54, 68, 84, 252};
    /* NTC 分压电路需要较长的采样时间，PB0 至少使用 71.5 个 ADC 时钟周期。 */
    unsigned window = input == ADC_PB0 ? ADC_SampleTime_71Cycles5 : 0;
    timed = rate != 0;
    if (timed) {
        if (rate > adc_clock / (cycles[window] + 2)) return -RT_EINVAL;
        while (window < 7 && rate <= adc_clock / (cycles[window + 1] + 2)) ++window;
        int rc = tim_prepare(TIM3, rate);
        if (rc != RT_EOK) return rc;
        TIM_SelectOutputTrigger(TIM3, TIM_TRGOSource_Update);
    }
    ADC_RegularChannelConfig(ADC1, input, 1, window);
    ADC_ClearFlag(ADC1, ADC_FLAG_EOC | ADC_FLAG_STRT);
    uint32_t trigger = timed ? ADC_ExternalTrigConv_T3_TRGO : ADC_ExternalTrigConv_None;
    ADC1->CTLR2 = (ADC1->CTLR2 & ~(ADC_EXTSEL | ADC_EXTTRIG | ADC_CONT)) | trigger;
    if (!timed && count > 1) ADC1->CTLR2 |= ADC_CONT;

    DMA_InitTypeDef cfg = {0};
    cfg.DMA_PeripheralBaseAddr = (uintptr_t)&ADC1->RDATAR;
    cfg.DMA_MemoryBaseAddr = (uintptr_t)samples;
    cfg.DMA_BufferSize = count;
    cfg.DMA_DIR = DMA_DIR_PeripheralSRC;
    cfg.DMA_MemoryInc = DMA_MemoryInc_Enable;
    cfg.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;
    cfg.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;
    cfg.DMA_Priority = DMA_Priority_High;
    dma_init(DMA1_Channel1);
    DMA_Init(DMA1_Channel1, &cfg);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    DMA_ITConfig(DMA1_Channel1, DMA_IT_TC | DMA_IT_TE, ENABLE);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);
    dma_start(DMA1_Channel1);
    ADC_DMACmd(ADC1, ENABLE);
    if (timed) {
        ADC_ExternalTrigConvCmd(ADC1, ENABLE);
        TIM_Cmd(TIM3, ENABLE);
    } else ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    uint32_t hz = timed ? rate : adc_clock / cycles[window];
    uint32_t ms = ((uint32_t)count * 1000U + hz - 1U) / hz;
    int rc = rt_sem_take(&complete, rt_tick_from_millisecond(ms + ms / 50U + 20U));
    rt_base_t irq = rt_hw_interrupt_disable();
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    if (rc == RT_EOK) rc = dma_result;
    else stop_sampling();
    DMA_ClearFlag(DMA1_FLAG_GL1);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);
    rt_hw_interrupt_enable(irq);
    if (rc < 0) {
        rt_sem_control(&complete, RT_IPC_CMD_RESET, RT_NULL);
        adc_clock = 0;
    } else if (timed || count > 1) {
        /* 最后一个在途转换结束后才允许切换输入。 */
        rt_hw_us_delay(1U + cycles[window] * 1000000U / adc_clock);
    }
    return rc;
}

int adc_capture(adc_input_t input, uint16_t *samples, size_t count, uint32_t rate_hz)
{
    extern const char _eusrstack[];
    uintptr_t addr = (uintptr_t)samples, end = (uintptr_t)_eusrstack;
    if ((input != ADC_PA1 && input != ADC_PB0 && input != ADC_PB1) || !count || count > UINT16_MAX ||
        (addr & 1U) || addr < SRAM_BASE || addr >= end || count > (end - addr) / 2U) return -RT_EINVAL;
    /* 部分 V307 批次的 DMA1_CH1 在地址低 16 位回卷时不会进位。
     * 波形识别需要连续样点，故在启动前拒绝跨页帧。 */
    if ((addr & 0xffffU) + count * sizeof(*samples) > 0x10000U) return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    rc = prepare();
    if (rc == RT_EOK && input == ADC_PB0) gpio_config(GPIOB, GPIO_Pin_0, GPIO_Mode_AIN);
    if (rc == RT_EOK) rc = transfer(input, samples, count, rate_hz);
    rt_mutex_release(&mutex);
    return rc;
}

int adc_read(adc_input_t input)
{
    uint16_t code;
    int rc = adc_capture(input, &code, 1, 0);
    return rc == RT_EOK ? code : rc;
}
