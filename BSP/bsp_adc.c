#include "bsp_adc.h"
#include "bsp_analog_pin.h"
#include "bsp_adc_timing_internal.h"
#include <rtthread.h>

#define ADC_EXTERNAL_CHANNELS 16U
#define ADC_CHANNEL_COUNT 18U
#define ADC_INTERNAL_MASK ((1UL << 16) | (1UL << 17))
#define ADC_MAX_CLOCK_HZ 14000000UL
#define ADC_FULL_SCALE 4095U
/* 轮询上限也适用于 RT-Thread 调度器尚未启动的场合。 */
#define ADC_POLL_LIMIT 100000UL
#define TOUCH_CHARGE_TIME 0x10U
#define TOUCH_DISCHARGE_TIME 0x08U

enum { ADC_OFF, ADC_READY, ADC_FAULT };
typedef struct {
    volatile uint8_t lock;
    uint8_t phase;
    uint8_t sample_time[ADC_CHANNEL_COUNT];
    int16_t offset;
    uint32_t channels;
} adc_state_t;
static adc_state_t states[2];
/* 引用在开始校准之前取得，允许两个实例同时校准、失败或释放。 */
static uint8_t clock_users;
static uint32_t saved_prescaler;

static adc_state_t *state_for(ADC_TypeDef *adc)
{
    if (adc == ADC1) return &states[0];
    if (adc == ADC2) return &states[1];
    return NULL;
}

static int valid_channel(ADC_TypeDef *adc, uint8_t channel)
{
    return channel < ADC_EXTERNAL_CHANNELS || (adc == ADC1 && channel < ADC_CHANNEL_COUNT);
}

static bsp_pin_t channel_pin(uint8_t channel)
{
    if (channel < 8) return (bsp_pin_t){GPIOA, 1U << channel};
    if (channel < 10) return (bsp_pin_t){GPIOB, 1U << (channel - 8)};
    return (bsp_pin_t){GPIOC, 1U << (channel - 10)};
}

static void settle_us(uint32_t us)
{
    /* 每次迭代至少一个 CPU 周期，不重配 RT-Thread 使用的 SysTick。 */
    uint32_t loops = (SystemCoreClock / 1000000UL + 1U) * us;
    while (loops--) __NOP();
}

static int wait_clear(volatile uint32_t *reg, uint32_t mask)
{
    for (uint32_t i = 0; i < ADC_POLL_LIMIT; i++)
        if (!(*reg & mask)) return 1;
    return 0;
}

static bsp_status_t calibrate(ADC_TypeDef *adc, int16_t *offset)
{
    /* 移植自 WCH Get_CalibrationValue。各硬件等待都设置上限。 */
    uint16_t values[10];
    for (unsigned i = 0; i < 10; i++) {
        ADC_ResetCalibration(adc);
        if (!wait_clear(&adc->CTLR2, ADC_RSTCAL)) return BSP_ERR_TIMEOUT;
        ADC_StartCalibration(adc);
        if (!wait_clear(&adc->CTLR2, ADC_CAL)) return BSP_ERR_TIMEOUT;
        uint16_t value = adc->RDATAR;
        unsigned j = i;
        while (j && values[j - 1] > value) {
            values[j] = values[j - 1];
            j--;
        }
        values[j] = value;
    }
    uint32_t sum = 0;
    for (unsigned i = 2; i < 8; i++) sum += values[i];
    *offset = 2048 - (int32_t)((sum + 3U) / 6U);
    return BSP_OK;
}

static void start_clock(ADC_TypeDef *adc)
{
    long level = bsp_irq_save();
    if (!clock_users) {
        saved_prescaler = RCC->CFGR0 & RCC_ADCPRE;
        RCC_ADCCLKConfig(RCC_PCLK2_Div8);
    }
    clock_users++;
    RCC_APB2PeriphClockCmd(adc == ADC1 ? RCC_APB2Periph_ADC1 : RCC_APB2Periph_ADC2, ENABLE);
    ADC_DeInit(adc);
    bsp_irq_restore(level);
}

static void stop_adc(ADC_TypeDef *adc)
{
    long level = bsp_irq_save();
    ADC_Cmd(adc, DISABLE);
    ADC_DeInit(adc);
    RCC_APB2PeriphClockCmd(adc == ADC1 ? RCC_APB2Periph_ADC1 : RCC_APB2Periph_ADC2, DISABLE);
    if (--clock_users == 0) RCC_ADCCLKConfig(saved_prescaler);
    bsp_irq_restore(level);
}

bsp_status_t bsp_adc_init(ADC_TypeDef *adc)
{
    adc_state_t *state = state_for(adc);
    RCC_ClocksTypeDef clocks;
    ADC_InitTypeDef init = {0};
    if (!state) return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_BUSY;
    if (state->phase == ADC_OFF) {
        RCC_GetClocksFreq(&clocks);
        if (clocks.PCLK2_Frequency > ADC_MAX_CLOCK_HZ * 8U) {
            status = BSP_ERR_UNSUPPORTED;
        } else {
            start_clock(adc);
            ADC_StructInit(&init);
            init.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
            ADC_Init(adc, &init);
            ADC_Cmd(adc, ENABLE);
            ADC_BufferCmd(adc, DISABLE);
            settle_us(2);
            status = calibrate(adc, &state->offset);
            if (status == BSP_OK) {
                ADC_BufferCmd(adc, ENABLE);
                state->phase = ADC_READY;
            } else stop_adc(adc);
        }
    }
    bsp_unlock(&state->lock);
    return status;
}

static bsp_status_t configure_channel(adc_state_t *state,
                                      uint8_t channel, uint8_t sample_time)
{
    uint32_t mask = 1UL << channel;
    if (!(state->channels & mask)) {
        if (channel < ADC_EXTERNAL_CHANNELS) {
            bsp_status_t result = bsp_analog_pin_claim(channel_pin(channel), BSP_ANALOG_INPUT);
            if (result != BSP_OK) return result;
        } else if (!(state->channels & ADC_INTERNAL_MASK)) {
            ADC_TempSensorVrefintCmd(ENABLE);
            settle_us(20);
        }
        state->channels |= mask;
    }
    state->sample_time[channel] = sample_time;
    return BSP_OK;
}

bsp_status_t bsp_adc_channel_init(ADC_TypeDef *adc, uint8_t channel, uint8_t sample_time)
{
    adc_state_t *state = state_for(adc);
    if (!state || !valid_channel(adc, channel) || sample_time > ADC_SampleTime_239Cycles5 ||
        (channel >= ADC_EXTERNAL_CHANNELS && sample_time != ADC_SampleTime_239Cycles5))
        return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t result = state->phase == ADC_READY ?
        configure_channel(state, channel, sample_time) : BSP_ERR_STATE;
    bsp_unlock(&state->lock);
    return result;
}

static bsp_status_t prepare_read(adc_state_t *state, uint8_t channel)
{
    if (state->phase != ADC_READY) return BSP_ERR_STATE;
    if (state->channels & (1UL << channel)) return BSP_OK;
    return configure_channel(state, channel, ADC_SampleTime_239Cycles5);
}

static void release_channel(adc_state_t *state, uint8_t channel)
{
    state->channels &= ~(1UL << channel);
    if (channel < ADC_EXTERNAL_CHANNELS)
        bsp_analog_pin_release(channel_pin(channel), BSP_ANALOG_INPUT);
    else if (!(state->channels & ADC_INTERNAL_MASK) && state->phase == ADC_READY)
        ADC_TempSensorVrefintCmd(DISABLE);
}

static bsp_status_t wait_data(ADC_TypeDef *adc, adc_state_t *state, uint16_t *value)
{
    for (uint32_t i = 0; i < ADC_POLL_LIMIT; i++) {
        if (ADC_GetFlagStatus(adc, ADC_FLAG_EOC) != RESET) {
            *value = ADC_GetConversionValue(adc);
            return BSP_OK;
        }
    }
    /* 丢弃失步的转换，防止迟到的 EOC 被下一次采样取走。 */
    stop_adc(adc);
    state->phase = ADC_FAULT;
    return BSP_ERR_TIMEOUT;
}

static uint16_t corrected(uint16_t raw, int16_t offset)
{
    int value = (int)raw + offset;
    if (!raw || value < 0) return 0;
    if (raw == ADC_FULL_SCALE || value > (int)ADC_FULL_SCALE) return ADC_FULL_SCALE;
    return (uint16_t)value;
}

bsp_status_t bsp_adc_read(ADC_TypeDef *adc, uint8_t channel, uint16_t *data, size_t count)
{
    adc_state_t *state = state_for(adc);
    if (!state || !valid_channel(adc, channel) || !data || ((uintptr_t)data & 1U) || !count)
        return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t result = prepare_read(state, channel);
    if (result == BSP_OK) {
        ADC_RegularChannelConfig(adc, channel, 1, state->sample_time[channel]);
        for (size_t i = 0; i < count; i++) {
            uint16_t value;
            ADC_ClearFlag(adc, ADC_FLAG_EOC);
            ADC_SoftwareStartConvCmd(adc, ENABLE);
            result = wait_data(adc, state, &value);
            if (result != BSP_OK) break;
            data[i] = corrected(value, state->offset);
        }
    }
    bsp_unlock(&state->lock);
    return result;
}

static void start_sample_timer(uint32_t frame_ms)
{
    TIM_DMACmd(TIM4, TIM_DMA_Update, ENABLE);
    TIM_Cmd(TIM4, ENABLE);
#ifdef __riscv
    if (!(__get_MSTATUS() & 8U)) return;
#endif
    /* DMA 持续采样；普通线程让出大部分帧时长，再轮询末尾。启动前或中断中仍可有限轮询。 */
    if (frame_ms > 1U && rt_thread_self() && !rt_interrupt_get_nest() && !rt_critical_level())
        rt_thread_mdelay((rt_int32_t)frame_ms - 1);
}

bsp_status_t bsp_adc_read_timed(ADC_TypeDef *adc, uint8_t channel, uint32_t rate_hz,
                                uint16_t *data, size_t count)
{
    adc_state_t *state = state_for(adc);
    uintptr_t dst = (uintptr_t)data;
    if (!state || !valid_channel(adc, channel) || !count || count > 4096U ||
        rate_hz < 2000U || rate_hz > 48000U || (dst & 1U) ||
        dst < SRAM_BASE || dst > SRAM_BASE + 65536U - count * sizeof(*data)) return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    uint32_t channel_mask = 1UL << channel;
    int new_channel = !(state->channels & channel_mask);
    uint8_t sample_time = new_channel ? ADC_SampleTime_239Cycles5 : state->sample_time[channel];
    bsp_status_t rc = state->phase == ADC_READY ? BSP_OK : BSP_ERR_STATE;
    RCC_ClocksTypeDef clocks;
    if (rc == BSP_OK) {
        RCC_GetClocksFreq(&clocks);
        uint32_t timer_clock = clocks.PCLK1_Frequency;
        if (timer_clock != clocks.HCLK_Frequency) timer_clock *= 2U;
        uint32_t ticks = timer_clock / rate_hz;
        /* 配置的采样周期 + 12.5 个转换周期，再加 4 个 ADCCLK 同步余量。
         * 必须在半个采样周期内结束；实际 CC4 到 DMA 更新有 3/4 周期。 */
        static const uint16_t guard_cycles[] = {18,24,30,45,58,72,88,256};
        if (ticks < 4U || ticks > UINT16_MAX || timer_clock % rate_hz ||
            clocks.PCLK2_Frequency / 8U < 2U * guard_cycles[sample_time] * rate_hz)
            rc = BSP_ERR_UNSUPPORTED;
        else rc = bsp_adc_trigger_claim((uint16_t)ticks, (uint16_t)count);
    }
    if (rc == BSP_OK) {
        rc = prepare_read(state, channel);
        if (rc != BSP_OK) bsp_adc_trigger_release();
    }
    if (rc == BSP_OK) {
        const uint32_t trigger_mask = ADC_EXTSEL | ADC_EXTTRIG;
        uint32_t saved = adc->CTLR2 & trigger_mask;
        ADC_RegularChannelConfig(adc, channel, 1, state->sample_time[channel]);
        ADC_ClearFlag(adc, ADC_FLAG_EOC | ADC_FLAG_STRT);
        adc->CTLR2 = (adc->CTLR2 & ~trigger_mask) | ADC_ExternalTrigConv_T4_CC4 | ADC_EXTTRIG;
        uint32_t polls = (clocks.HCLK_Frequency / rate_hz + 1U) * ((uint32_t)count + 2U);
        uint32_t frame_ms = (uint32_t)count * 1000U / rate_hz;
        rc = bsp_adc_frame_dma(adc, data, (uint16_t)count, polls, start_sample_timer, frame_ms);
        if (rc == BSP_OK) {
            /* DMA 已关闭。单脉冲模式在下一次更新停表，强制打开 TIM8 的
             * 门输出即可完成尾部验证，无需靠已停止的外部计数钟改变比较状态。 */
            uint16_t discard;
            ADC_ClearFlag(adc, ADC_FLAG_EOC);
            TIM_SelectOnePulseMode(TIM4, TIM_OPMode_Single);
            TIM_ForcedOC1Config(TIM8, TIM_ForcedAction_Active);
            rc = wait_data(adc, state, &discard);
        }
        bsp_adc_trigger_release();
        if (state->phase == ADC_READY) {
            ADC_ExternalTrigConvCmd(adc, DISABLE);
            uint32_t adc_hz = clocks.PCLK2_Frequency / 8U;
            settle_us(1U + 256000000U / adc_hz); /* 等待可能在途的最后一次转换 */
            (void)adc->RDATAR;
            ADC_ClearFlag(adc, ADC_FLAG_EOC | ADC_FLAG_STRT);
            adc->CTLR2 = (adc->CTLR2 & ~trigger_mask) | saved;
        }
        if (rc == BSP_OK)
            for (size_t i = 0; i < count; ++i) data[i] = corrected(data[i], state->offset);
    }
    if (rc != BSP_OK && new_channel && (state->channels & channel_mask))
        release_channel(state, channel);
    bsp_unlock(&state->lock);
    return rc;
}

bsp_status_t bsp_adc_touch_read(ADC_TypeDef *adc, uint8_t channel, uint16_t *value)
{
    adc_state_t *state = state_for(adc);
    if (!state || channel >= ADC_EXTERNAL_CHANNELS || !value || ((uintptr_t)value & 1U))
        return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    int newly_acquired = !(state->channels & (1UL << channel));
    bsp_status_t result = prepare_read(state, channel);
    bsp_pin_t pin = channel_pin(channel);
    if (result == BSP_OK) {
        result = bsp_analog_touch_begin(pin);
        if (result != BSP_OK && newly_acquired) {
            bsp_analog_pin_release(pin, BSP_ANALOG_INPUT);
            state->channels &= ~(1UL << channel);
        }
    }
    if (result == BSP_OK) {
        adc->CTLR1 |= ADC_TKENABLE;
        ADC_RegularChannelConfig(adc, channel, 1, ADC_SampleTime_7Cycles5);
        ADC_ClearFlag(adc, ADC_FLAG_EOC);
        adc->IDATAR1 = TOUCH_CHARGE_TIME;
        adc->RDATAR = TOUCH_DISCHARGE_TIME;
        result = wait_data(adc, state, value);
        if (result == BSP_OK) adc->CTLR1 &= ~ADC_TKENABLE;
        bsp_analog_touch_end(pin);
    }
    bsp_unlock(&state->lock);
    return result;
}

bsp_status_t bsp_adc_channel_de_init(ADC_TypeDef *adc, uint8_t channel)
{
    adc_state_t *state = state_for(adc);
    if (!state || !valid_channel(adc, channel)) return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t result = BSP_ERR_STATE;
    if (state->channels & (1UL << channel)) {
        release_channel(state, channel);
        result = BSP_OK;
    }
    bsp_unlock(&state->lock);
    return result;
}

bsp_status_t bsp_adc_de_init(ADC_TypeDef *adc)
{
    adc_state_t *state = state_for(adc);
    if (!state) return BSP_ERR_ARG;
    if (bsp_try_lock(&state->lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t result = BSP_ERR_STATE;
    if (state->phase != ADC_OFF) {
        if (state->phase == ADC_READY) stop_adc(adc);
        state->phase = ADC_OFF;
        for (uint8_t i = 0; i < ADC_CHANNEL_COUNT; i++)
            if (state->channels & (1UL << i)) release_channel(state, i);
        result = BSP_OK;
    }
    bsp_unlock(&state->lock);
    return result;
}
