#include "bsp_tim.h"
#include "bsp_dac_playback_internal.h"
#include "bsp_resource.h"
#include "bsp_adc_timing_internal.h"

typedef struct {
    TIM_TypeDef *tim;
    uint32_t clock;
    uint8_t apb2;
    uint8_t routes;       /* 可选路由编号位图。基本定时器为 0。 */
    uint8_t remap_shift;
    uint8_t remap_width;
    uint8_t pcfr2;
    uint8_t pins[4][4];   /* 每个 route 下 CH1~CH4 的物理引脚。 */
} timer_info_t;

#define PA(n) (n)
#define PB(n) (16 + (n))
#define PC(n) (32 + (n))
#define PD(n) (48 + (n))
#define PE(n) (64 + (n))
static const timer_info_t timers[] = {
    {TIM1, RCC_APB2Periph_TIM1, 1, 0xB, 6, 2, 0, {
        {PA(8),PA(9),PA(10),PA(11)}, {PA(8),PA(9),PA(10),PA(11)}, {0}, {PE(9),PE(11),PE(13),PE(14)}}},
    {TIM2, RCC_APB1Periph_TIM2, 0, 0xF, 8, 2, 0, {
        {PA(0),PA(1),PA(2),PA(3)}, {PA(15),PB(3),PA(2),PA(3)},
        {PA(0),PA(1),PB(10),PB(11)}, {PA(15),PB(3),PB(10),PB(11)}}},
    {TIM3, RCC_APB1Periph_TIM3, 0, 0xD, 10, 2, 0, {
        {PA(6),PA(7),PB(0),PB(1)}, {0}, {PB(4),PB(5),PB(0),PB(1)}, {PC(6),PC(7),PC(8),PC(9)}}},
    {TIM4, RCC_APB1Periph_TIM4, 0, 3, 12, 1, 0, {
        {PB(6),PB(7),PB(8),PB(9)}, {PD(12),PD(13),PD(14),PD(15)}}},
    {TIM5, RCC_APB1Periph_TIM5, 0, 1, 0, 0, 0, {{PA(0),PA(1),PA(2),PA(3)}}},
    {TIM6, RCC_APB1Periph_TIM6, 0, 0, 0, 0, 0, {{0}}},
    {TIM7, RCC_APB1Periph_TIM7, 0, 0, 0, 0, 0, {{0}}},
    {TIM8, RCC_APB2Periph_TIM8, 1, 3, 2, 1, 1, {
        {PC(6),PC(7),PC(8),PC(9)}, {PB(6),PB(7),PB(8),PC(13)}}},
    {TIM9, RCC_APB2Periph_TIM9, 1, 0xF, 3, 2, 1, {
        {PA(2),PA(3),PA(4),PC(4)}, {PA(2),PA(3),PA(4),PC(14)},
        {PD(9),PD(11),PD(13),PD(15)}, {PD(9),PD(11),PD(13),PD(15)}}},
    {TIM10, RCC_APB2Periph_TIM10, 1, 0xF, 5, 2, 1, {
        {PB(8),PB(9),PC(3),PC(11)}, {PB(3),PB(4),PB(5),PC(15)},
        {PD(1),PD(3),PD(5),PD(7)}, {PD(1),PD(3),PD(5),PD(7)}}}
};

enum { TIMER_IDLE, TIMER_TIMEBASE, TIMER_ENCODER, TIMER_ADC_TRIGGER, TIMER_DAC_CLOCK };

typedef struct {
    uint8_t active;
    uint8_t channels;
    uint8_t route;
    uint8_t previous_route;
    uint8_t captures;
    uint8_t auto_stop;      /* 由 PWM 通道启动的时基随最后一个通道释放。 */
    uint8_t phase_owner;    /* 短脉冲序列独占 CNT，0 表示允许共享。 */
    uint8_t pulse_inputs;   /* 每路脉宽输入还使用同组配对通道的比较寄存器。 */
    uint16_t encoder_previous;
} timer_state_t;
#define TIMER_COUNT (sizeof timers / sizeof timers[0])
static timer_state_t states[TIMER_COUNT];
static GPIO_TypeDef *const ports[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE};

static const timer_info_t *info_for(TIM_TypeDef *tim)
{
    for (size_t i = 0; i < TIMER_COUNT; i++)
        if (timers[i].tim == tim) return &timers[i];
    return NULL;
}

static void clock_cmd(const timer_info_t *info, FunctionalState enable)
{
    if (info->apb2) RCC_APB2PeriphClockCmd(info->clock, enable);
    else RCC_APB1PeriphClockCmd(info->clock, enable);
}

bsp_status_t bsp_tim_init(TIM_TypeDef *tim, uint16_t psc, uint16_t arr)
{
    const timer_info_t *info = info_for(tim);
    if (!info || !arr) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (!state->active) {
        TIM_TimeBaseInitTypeDef config = {0};
        config.TIM_Prescaler = psc;
        config.TIM_Period = arr;
        clock_cmd(info, ENABLE);
        TIM_DeInit(tim);
        TIM_TimeBaseInit(tim, &config);
        TIM_ClearFlag(tim, TIM_FLAG_Update);
        TIM_Cmd(tim, ENABLE);
        state->active = TIMER_TIMEBASE;
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

static bsp_status_t period_for_hz(const timer_info_t *info, uint32_t frequency_hz,
                                  uint16_t *psc, uint16_t *arr)
{
    if (!info || !frequency_hz) return BSP_ERR_ARG;
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    uint32_t hz = info->apb2 ? clocks.PCLK2_Frequency : clocks.PCLK1_Frequency;
    if (hz != clocks.HCLK_Frequency) hz *= 2U;
    if (frequency_hz > hz / 2U) return BSP_ERR_ARG;
    uint32_t cycles = (hz + frequency_hz / 2U) / frequency_hz;
    uint32_t divider = (cycles + 65535U) / 65536U;
    uint32_t divisor_hz = divider * frequency_hz;
    uint32_t ticks = (hz + divisor_hz / 2U) / divisor_hz;
    *psc = (uint16_t)(divider - 1U);
    *arr = (uint16_t)(ticks - 1U);
    return BSP_OK;
}

bsp_status_t bsp_tim_init_hz(TIM_TypeDef *tim, uint32_t frequency_hz)
{
    uint16_t psc, arr;
    bsp_status_t result = period_for_hz(info_for(tim), frequency_hz, &psc, &arr);
    return result == BSP_OK ? bsp_tim_init(tim, psc, arr) : result;
}

bsp_status_t bsp_tim_counter(TIM_TypeDef *tim, uint16_t *value)
{
    const timer_info_t *info = info_for(tim);
    if (!info || !value) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (states[info - timers].active >= TIMER_ADC_TRIGGER) result = BSP_ERR_BUSY;
    else if (states[info - timers].active) {
        *value = TIM_GetCounter(tim);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_take_update(TIM_TypeDef *tim, uint8_t *updated)
{
    const timer_info_t *info = info_for(tim);
    if (!info || !updated) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (states[info - timers].active >= TIMER_ADC_TRIGGER) result = BSP_ERR_BUSY;
    else if (states[info - timers].active) {
        *updated = TIM_GetFlagStatus(tim, TIM_FLAG_Update) != RESET;
        if (*updated) TIM_ClearFlag(tim, TIM_FLAG_Update);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

static bsp_pin_t channel_pin(const timer_info_t *info, uint8_t route, uint8_t channel)
{
    uint8_t code = info->pins[route][channel - 1];
    return (bsp_pin_t){ports[code / 16], 1U << (code % 16)};
}

static uint8_t set_route(const timer_info_t *info, uint8_t route)
{
    if (!info->remap_width) return 0;
    volatile uint32_t *reg = info->pcfr2 ? &AFIO->PCFR2 : &AFIO->PCFR1;
    uint32_t mask = ((1UL << info->remap_width) - 1U) << info->remap_shift;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    uint32_t old = *reg;
    *reg = (old & ~mask) | ((uint32_t)route << info->remap_shift);
    return (uint8_t)((old & mask) >> info->remap_shift);
}

/* 调用方在短临界区内检查 route 和通道号。两种通道模式共用同一张占用表。 */
static uint8_t paired_mask(uint8_t channels)
{
    return ((channels & 0x05U) << 1) | ((channels & 0x0AU) >> 1);
}

static uint8_t pwm_mask(const timer_state_t *state)
{
    return state->channels & ~(state->captures | state->pulse_inputs);
}

static bsp_status_t claim_channel(const timer_info_t *info, timer_state_t *state,
                                  uint8_t route, uint8_t channel, bsp_pin_t *pin)
{
    if (state->active == TIMER_IDLE) return BSP_ERR_STATE;
    if (state->active != TIMER_TIMEBASE) return BSP_ERR_BUSY;
    uint8_t bit = 1U << (channel - 1);
    if (state->phase_owner || ((state->channels | paired_mask(state->pulse_inputs)) & bit) ||
        (state->channels && state->route != route)) return BSP_ERR_BUSY;
    *pin = channel_pin(info, route, channel);
    bsp_status_t result = bsp_pins_claim(state, pin, 1);
    if (result == BSP_OK) {
        if (!state->channels) {
            state->previous_route = set_route(info, route);
            state->route = route;
        }
        state->channels |= bit;
    }
    return result;
}

typedef void (*oc_init_fn)(TIM_TypeDef *, TIM_OCInitTypeDef *);
typedef void (*oc_preload_fn)(TIM_TypeDef *, uint16_t);
static const oc_init_fn oc_init[] = {TIM_OC1Init, TIM_OC2Init, TIM_OC3Init, TIM_OC4Init};
static const oc_preload_fn oc_preload[] = {TIM_OC1PreloadConfig, TIM_OC2PreloadConfig,
                                            TIM_OC3PreloadConfig, TIM_OC4PreloadConfig};

static uint32_t pulse_for(TIM_TypeDef *tim, uint16_t permille)
{
    return (((uint32_t)tim->ATRLR + 1U) * permille + 500U) / 1000U;
}

bsp_status_t bsp_tim_pwm_init_on(TIM_TypeDef *tim, uint8_t channel, uint8_t route, uint16_t duty)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || route > 3 || duty > 1000) return BSP_ERR_ARG;
    if (!(info->routes & (1U << route))) return BSP_ERR_UNSUPPORTED;
    timer_state_t *state = &states[info - timers];
    bsp_pin_t pin;
    long level = bsp_irq_save();
    bsp_status_t result = claim_channel(info, state, route, channel, &pin);
    if (result == BSP_OK) {
        uint32_t pulse = pulse_for(tim, duty);
        TIM_OCInitTypeDef config = {0};
        config.TIM_OCMode = pulse > UINT16_MAX ? TIM_ForcedAction_Active : TIM_OCMode_PWM1;
        config.TIM_OutputState = TIM_OutputState_Enable;
        config.TIM_Pulse = (uint16_t)pulse;
        /* 初次写入直接生效，避免曾释放过的通道输出上一次的比较值。 */
        oc_preload[channel - 1](tim, TIM_OCPreload_Disable);
        oc_init[channel - 1](tim, &config);
        oc_preload[channel - 1](tim, TIM_OCPreload_Enable);
        if (info->apb2) TIM_CtrlPWMOutputs(tim, ENABLE);
        bsp_pin_config(pin, GPIO_Mode_AF_PP);
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_pwm_init(TIM_TypeDef *tim, uint8_t channel, uint16_t duty)
{
    return bsp_tim_pwm_init_on(tim, channel, 0, duty);
}

bsp_status_t bsp_tim_pwm_init_hz(TIM_TypeDef *tim, uint8_t channel, uint32_t hz, uint16_t duty)
{
    const timer_info_t *info = info_for(tim);
    uint16_t psc, arr;
    bsp_status_t rc = period_for_hz(info, hz, &psc, &arr);
    if (rc != BSP_OK) return rc;
    if (channel < 1 || channel > 4 || duty > 1000) return BSP_ERR_ARG;
    if (!info->routes) return BSP_ERR_UNSUPPORTED;
    timer_state_t *s = &states[info - timers];
    long level = bsp_irq_save();
    int created = s->active == TIMER_IDLE;
    if (created) rc = bsp_tim_init(tim, psc, arr);
    else if (s->active != TIMER_TIMEBASE || !s->auto_stop || tim->PSC != psc || tim->ATRLR != arr)
        rc = BSP_ERR_BUSY;
    if (rc == BSP_OK) rc = bsp_tim_pwm_init(tim, channel, duty);
    if (created) {
        if (rc == BSP_OK) s->auto_stop = 1;
        else (void)bsp_tim_de_init(tim);
    }
    bsp_irq_restore(level);
    return rc;
}

static void write_pwm(TIM_TypeDef *tim, uint8_t channel, uint16_t duty)
{
    volatile uint16_t *compares[] = {&tim->CH1CVR, &tim->CH2CVR, &tim->CH3CVR, &tim->CH4CVR};
    uint32_t pulse = pulse_for(tim, duty);
    *compares[channel - 1] = (uint16_t)pulse;
    /* ARR=65535 时全高需要强制有效；只有模式变化才改 CHCTLR。 */
    volatile uint16_t *control = channel <= 2 ? &tim->CHCTLR1 : &tim->CHCTLR2;
    unsigned shift = ((channel - 1) & 1U) * 8U;
    uint16_t mode = pulse > UINT16_MAX ? TIM_ForcedAction_Active : TIM_OCMode_PWM1;
    uint16_t old = *control;
    uint16_t next = (old & ~(TIM_OC1M << shift)) | (mode << shift);
    if (next != old) *control = next;
}

static bsp_status_t update_pwm(TIM_TypeDef *tim, uint8_t channel, uint16_t duty, int immediate)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || duty > 1000) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->active == TIMER_TIMEBASE && (pwm_mask(state) & (1U << (channel - 1)))) {
        if (immediate) oc_preload[channel - 1](tim, TIM_OCPreload_Disable);
        write_pwm(tim, channel, duty);
        if (immediate) oc_preload[channel - 1](tim, TIM_OCPreload_Enable);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_pwm_write(TIM_TypeDef *tim, uint8_t channel, uint16_t duty)
{
    return update_pwm(tim, channel, duty, 0);
}

bsp_status_t bsp_tim_pwm_write_immediate(TIM_TypeDef *tim, uint8_t channel, uint16_t duty)
{
    return update_pwm(tim, channel, duty, 1);
}

bsp_status_t bsp_tim_pwm_lock_phase(TIM_TypeDef *tim, uint8_t channel, uint8_t lock)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || lock > 1) return BSP_ERR_ARG;
    timer_state_t *s = &states[info - timers];
    uint8_t bit = 1U << (channel - 1);
    long level = bsp_irq_save();
    bsp_status_t rc = BSP_ERR_STATE;
    if (s->active == TIMER_TIMEBASE && (pwm_mask(s) & bit)) {
        rc = BSP_OK;
        if (lock && (s->channels != bit || s->phase_owner)) rc = BSP_ERR_BUSY;
        else if (lock) s->phase_owner = channel;
        else if (s->phase_owner == channel) s->phase_owner = 0;
    }
    bsp_irq_restore(level);
    return rc;
}

bsp_status_t bsp_tim_pwm_restart(TIM_TypeDef *tim, uint8_t channel, uint16_t duty)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || duty > 1000) return BSP_ERR_ARG;
    timer_state_t *s = &states[info - timers];
    long level = bsp_irq_save();
    uint8_t bit = 1U << (channel - 1);
    bsp_status_t rc = BSP_ERR_STATE;
    if (s->active == TIMER_TIMEBASE && (pwm_mask(s) & bit)) {
        if (s->channels != bit) {
            bsp_irq_restore(level);
            return BSP_ERR_BUSY;
        }
        TIM_Cmd(tim, DISABLE);
        TIM_SetCounter(tim, 0);
        rc = bsp_tim_pwm_write_immediate(tim, channel, duty);
        TIM_Cmd(tim, ENABLE);
    }
    bsp_irq_restore(level);
    return rc;
}

bsp_status_t bsp_tim_pwm_set_frequency(TIM_TypeDef *tim, uint8_t channel,
                                       uint32_t frequency_hz, uint16_t duty)
{
    const timer_info_t *info = info_for(tim);
    uint16_t psc, arr;
    if (channel < 1 || channel > 4 || duty > 1000) return BSP_ERR_ARG;
    bsp_status_t result = period_for_hz(info, frequency_hz, &psc, &arr);
    if (result != BSP_OK) return result;
    uint8_t bit = 1U << (channel - 1U);
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    if (state->active != TIMER_TIMEBASE || !(pwm_mask(state) & bit)) result = BSP_ERR_STATE;
    else if (state->channels != bit) {
        if (tim->PSC == psc && tim->ATRLR == arr)
            result = bsp_tim_pwm_write_immediate(tim, channel, duty);
        else result = BSP_ERR_BUSY;
    }
    else {
        TIM_Cmd(tim, DISABLE);
        TIM_PrescalerConfig(tim, psc, TIM_PSCReloadMode_Update);
        TIM_SetAutoreload(tim, arr);
        write_pwm(tim, channel, duty);
        TIM_GenerateEvent(tim, TIM_EventSource_Update);
        TIM_ClearFlag(tim, TIM_FLAG_Update);
        TIM_Cmd(tim, ENABLE);
    }
    bsp_irq_restore(level);
    return result;
}

static void reset_channel(TIM_TypeDef *tim, uint8_t channel)
{
    unsigned shift = (channel - 1U) * 4U;
    tim->CCER &= ~(0xFU << shift);
    /* 捕获滤波位与输出比较的清零/预装位复用。释放时仅复位本通道字节。 */
    volatile uint16_t *control = channel <= 2 ? &tim->CHCTLR1 : &tim->CHCTLR2;
    *control &= ~(0xFFU << (((channel - 1U) & 1U) * 8U));
    TIM_ClearFlag(tim, (TIM_FLAG_CC1 | TIM_FLAG_CC1OF) << (channel - 1));
}

static void stop_timebase(const timer_info_t *info, timer_state_t *state)
{
    TIM_DeInit(info->tim);
    clock_cmd(info, DISABLE);
    state->active = TIMER_IDLE;
    state->auto_stop = 0;
    state->phase_owner = 0;
}

static void release_channel(const timer_info_t *info, timer_state_t *state, uint8_t channel)
{
    bsp_pin_t pin = channel_pin(info, state->route, channel);
    reset_channel(info->tim, channel);
    bsp_pin_floating(pin);
    bsp_pins_release(state, &pin, 1);
    state->channels &= ~(1U << (channel - 1));
    if (state->phase_owner == channel) state->phase_owner = 0;
    state->captures &= ~(1U << (channel - 1));
    if (state->pulse_inputs & (1U << (channel - 1))) {
        reset_channel(info->tim, ((channel - 1U) ^ 1U) + 1U);
        state->pulse_inputs &= ~(1U << (channel - 1));
    }
    if (!state->channels) {
        if (info->apb2) TIM_CtrlPWMOutputs(info->tim, DISABLE);
        set_route(info, state->previous_route);
        if (state->auto_stop) stop_timebase(info, state);
    }
}

bsp_status_t bsp_tim_pwm_de_init(TIM_TypeDef *tim, uint8_t channel)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->active == TIMER_TIMEBASE && (pwm_mask(state) & (1U << (channel - 1)))) {
        release_channel(info, state, channel);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

static void stop_timer(const timer_info_t *info, timer_state_t *state)
{
    TIM_Cmd(info->tim, DISABLE);
    state->auto_stop = 0;
    for (uint8_t i = 1; i <= 4; i++)
        if (state->channels & (1U << (i - 1))) release_channel(info, state, i);
    stop_timebase(info, state);
}

bsp_status_t bsp_tim_de_init(TIM_TypeDef *tim)
{
    const timer_info_t *info = info_for(tim);
    if (!info) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->active >= TIMER_ADC_TRIGGER || state->auto_stop) result = BSP_ERR_BUSY;
    else if (state->active) {
        stop_timer(info, state);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_dac_hold_timer(uint32_t rate, float *actual)
{
    const timer_info_t *info = info_for(TIM7);
    timer_state_t *s = &states[info - timers];
    uint16_t psc, arr;
    if (s->active || (RCC->APB1PCENR & info->clock)) return BSP_ERR_BUSY;
    bsp_status_t rc = period_for_hz(info, rate, &psc, &arr);
    if (rc != BSP_OK) return rc;
    clock_cmd(info, ENABLE);
    TIM_DeInit(TIM7);
    TIM_TimeBaseInitTypeDef cfg = {0};
    cfg.TIM_Prescaler = psc;
    cfg.TIM_Period = arr;
    TIM_TimeBaseInit(TIM7, &cfg);
    TIM_ClearFlag(TIM7, TIM_FLAG_Update);
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    uint32_t timer_hz = clocks.PCLK1_Frequency * (clocks.PCLK1_Frequency == clocks.HCLK_Frequency ? 1U : 2U);
    *actual = ((float)timer_hz / (psc + 1U)) / (arr + 1U);
    s->active = TIMER_DAC_CLOCK;
    return BSP_OK;
}

void bsp_dac_release_timer(void)
{
    const timer_info_t *info = info_for(TIM7);
    stop_timer(info, &states[info - timers]);
}

bsp_status_t bsp_adc_trigger_claim(uint16_t period_ticks, uint16_t samples)
{
    const timer_info_t *info = info_for(TIM4);
    const timer_info_t *gate = info_for(TIM8);
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t rc = BSP_ERR_BUSY;
    if (state->active == TIMER_IDLE && states[gate - timers].active == TIMER_IDLE &&
        !(RCC->APB1PCENR & info->clock) && !(RCC->APB2PCENR & gate->clock)) {
        state->active = TIMER_ADC_TRIGGER;
        states[gate - timers].active = TIMER_ADC_TRIGGER;
        clock_cmd(info, ENABLE);
        clock_cmd(gate, ENABLE);
        TIM_DeInit(TIM4);
        TIM_DeInit(TIM8);
        /* 计数从零开始：周期 1/4 处的内部 CC4 触发 ADC，在下一次溢出时由 DMA 读取。
         * CC4E 保持清零，PB9 的板载 I2C 总线不受影响。 */
        TIM4->ATRLR = period_ticks - 1U;
        TIM_OCInitTypeDef compare = {0};
        compare.TIM_OCMode = TIM_OCMode_PWM2;
        compare.TIM_OutputState = TIM_OutputState_Disable;
        compare.TIM_Pulse = period_ticks / 4U;
        TIM_OC4Init(TIM4, &compare);
        TIM_GenerateEvent(TIM4, TIM_EventSource_Update);
        TIM_ClearFlag(TIM4, TIM_FLAG_Update | TIM_FLAG_CC4);

        /* RM 表 14-2、15-2：TIM8 的 ITR2 接 TIM4，TIM4 的 ITR3 接 TIM8。
         * 每次真实溢出让 TIM8 加一。OC1REF 在 CNT < samples 时为高；
         * 到达目标后 TIM4 硬件停止计数，线程迟醒也不会再触发或补采。 */
        TIM8->ATRLR = UINT16_MAX;
        TIM_OCInitTypeDef window = {0};
        window.TIM_OCMode = TIM_OCMode_PWM1;
        window.TIM_Pulse = samples;
        TIM_OC1Init(TIM8, &window);
        TIM_SelectOutputTrigger(TIM8, TIM_TRGOSource_OC1Ref);
        TIM_ITRxExternalClockConfig(TIM8, TIM_TS_ITR2);
        TIM_SelectOutputTrigger(TIM4, TIM_TRGOSource_Update);
        TIM_SelectInputTrigger(TIM4, TIM_TS_ITR3);
        TIM_SelectSlaveMode(TIM4, TIM_SlaveMode_Gated);
        TIM_Cmd(TIM8, ENABLE);
        rc = BSP_OK;
    }
    bsp_irq_restore(level);
    return rc;
}

void bsp_adc_trigger_release(void)
{
    const timer_info_t *info = info_for(TIM4);
    const timer_info_t *gate = info_for(TIM8);
    long level = bsp_irq_save();
    stop_timer(info, &states[info - timers]);
    stop_timer(gate, &states[gate - timers]);
    bsp_irq_restore(level);
}
static void configure_capture(TIM_TypeDef *tim, uint8_t channel, uint16_t selection,
                              uint16_t polarity, uint8_t filter)
{
    TIM_ICInitTypeDef capture = {0};
    capture.TIM_Channel = (channel - 1U) * 4U;
    capture.TIM_ICPolarity = polarity;
    capture.TIM_ICSelection = selection;
    capture.TIM_ICPrescaler = TIM_ICPSC_DIV1;
    capture.TIM_ICFilter = filter;
    TIM_ICInit(tim, &capture);
}

bsp_status_t bsp_tim_capture_init(TIM_TypeDef *tim, uint8_t channel, uint8_t route,
                                  uint16_t polarity, uint8_t filter)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || route > 3 || filter > 15 ||
        (polarity != TIM_ICPolarity_Rising && polarity != TIM_ICPolarity_Falling))
        return BSP_ERR_ARG;
    if (!(info->routes & (1U << route))) return BSP_ERR_UNSUPPORTED;
    timer_state_t *state = &states[info - timers];
    bsp_pin_t pin;
    long level = bsp_irq_save();
    bsp_status_t result = claim_channel(info, state, route, channel, &pin);
    if (result == BSP_OK) {
        bsp_pin_config(pin, GPIO_Mode_IN_FLOATING);
        TIM_ClearFlag(tim, (TIM_FLAG_CC1 | TIM_FLAG_CC1OF) << (channel - 1));
        configure_capture(tim, channel, TIM_ICSelection_DirectTI, polarity, filter);
        state->captures |= 1U << (channel - 1);
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_capture_set_edge(TIM_TypeDef *tim, uint8_t channel, uint16_t polarity)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 ||
        (polarity != TIM_ICPolarity_Rising && polarity != TIM_ICPolarity_Falling))
        return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->captures & (1U << (channel - 1))) {
        unsigned shift = (channel - 1U) * 4U;
        TIM_CCxCmd(tim, shift, TIM_CCx_Disable);
        tim->CCER = (tim->CCER & ~(TIM_CC1P << shift)) | (polarity << shift);
        TIM_ClearFlag(tim, (TIM_FLAG_CC1 | TIM_FLAG_CC1OF) << (channel - 1));
        TIM_CCxCmd(tim, shift, TIM_CCx_Enable);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

static uint16_t (*const read_capture[])(TIM_TypeDef *) =
    {TIM_GetCapture1, TIM_GetCapture2, TIM_GetCapture3, TIM_GetCapture4};

bsp_status_t bsp_tim_capture_read(TIM_TypeDef *tim, uint8_t channel, uint16_t *tick)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || !tick) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->captures & (1U << (channel - 1))) {
        uint16_t flag = TIM_FLAG_CC1 << (channel - 1);
        uint16_t overrun = TIM_FLAG_CC1OF << (channel - 1);
        result = BSP_ERR_BUSY;
        if (tim->INTFR & flag) {
            /* 读取 CHxCVR 自动清相应的 CCxIF，后续到达的新边沿保持可见。 */
            *tick = read_capture[channel - 1](tim);
            if (tim->INTFR & overrun) {
                TIM_ClearFlag(tim, overrun);
                result = BSP_ERR_IO;
            } else result = BSP_OK;
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_capture_de_init(TIM_TypeDef *tim, uint8_t channel)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->captures & (1U << (channel - 1))) {
        release_channel(info, state, channel);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_encoder_init(TIM_TypeDef *tim, uint8_t route, uint8_t filter)
{
    const timer_info_t *info = info_for(tim);
    if (!info || route > 3 || filter > 15) return BSP_ERR_ARG;
    if (!(info->routes & (1U << route))) return BSP_ERR_UNSUPPORTED;
    timer_state_t *state = &states[info - timers];
    bsp_pin_t pins[] = {channel_pin(info, route, 1), channel_pin(info, route, 2)};
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (state->active == TIMER_IDLE) {
        result = bsp_pins_claim(state, pins, 2);
        if (result == BSP_OK) {
            TIM_TimeBaseInitTypeDef base = {0};
            base.TIM_Period = UINT16_MAX;
            clock_cmd(info, ENABLE);
            TIM_DeInit(tim);
            TIM_TimeBaseInit(tim, &base);
            state->previous_route = set_route(info, route);
            state->route = route;
            state->channels = 3;
            for (uint8_t i = 0; i < 2; i++) {
                bsp_pin_config(pins[i], GPIO_Mode_IN_FLOATING);
                configure_capture(tim, i + 1U, TIM_ICSelection_DirectTI, TIM_ICPolarity_Rising, filter);
            }
            TIM_EncoderInterfaceConfig(tim, TIM_EncoderMode_TI12, TIM_ICPolarity_Rising, TIM_ICPolarity_Rising);
            TIM_SetCounter(tim, 0);
            TIM_ClearFlag(tim, TIM_FLAG_Update);
            TIM_Cmd(tim, ENABLE);
            state->encoder_previous = 0;
            state->active = TIMER_ENCODER;
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_encoder_delta(TIM_TypeDef *tim, int16_t *delta)
{
    const timer_info_t *info = info_for(tim);
    if (!info || !delta) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->active == TIMER_ENCODER) {
        uint16_t current = TIM_GetCounter(tim);
        *delta = (int16_t)(uint16_t)(current - state->encoder_previous);
        state->encoder_previous = current;
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_encoder_de_init(TIM_TypeDef *tim)
{
    const timer_info_t *info = info_for(tim);
    if (!info) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->active == TIMER_ENCODER) {
        stop_timer(info, state);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}
bsp_status_t bsp_tim_pulse_init(TIM_TypeDef *tim, uint8_t channel, uint8_t route, uint8_t filter)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || route > 3 || filter > 15) return BSP_ERR_ARG;
    if (!(info->routes & (1U << route))) return BSP_ERR_UNSUPPORTED;
    timer_state_t *state = &states[info - timers];
    uint8_t bit = 1U << (channel - 1);
    uint8_t partner = ((channel - 1U) ^ 1U) + 1U;
    bsp_pin_t pin;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (!(state->channels & paired_mask(bit))) {
        result = claim_channel(info, state, route, channel, &pin);
        if (result == BSP_OK) {
            bsp_pin_config(pin, GPIO_Mode_IN_FLOATING);
            uint16_t flags = (TIM_FLAG_CC1 | TIM_FLAG_CC1OF) << (channel - 1);
            flags |= (TIM_FLAG_CC1 | TIM_FLAG_CC1OF) << (partner - 1);
            TIM_ClearFlag(tim, flags);
            configure_capture(tim, channel, TIM_ICSelection_DirectTI, TIM_ICPolarity_Rising, filter);
            configure_capture(tim, partner, TIM_ICSelection_IndirectTI, TIM_ICPolarity_Falling, filter);
            state->pulse_inputs |= bit;
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_pulse_read(TIM_TypeDef *tim, uint8_t channel, uint16_t *width)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4 || !width) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    uint8_t bit = 1U << (channel - 1);
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->pulse_inputs & bit) {
        uint16_t flags = (bit | paired_mask(bit)) << 1;
        result = BSP_ERR_BUSY;
        if ((tim->INTFR & flags) == flags) {
            uint16_t rise = read_capture[channel - 1](tim);
            uint16_t fall = read_capture[(channel - 1) ^ 1](tim);
            uint16_t overruns = flags << 8;
            if (tim->INTFR & overruns) {
                TIM_ClearFlag(tim, overruns);
                result = BSP_ERR_IO;
            } else {
                *width = fall >= rise ? (uint32_t)fall - rise : (uint32_t)tim->ATRLR + 1U - rise + fall;
                result = BSP_OK;
            }
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_pulse_de_init(TIM_TypeDef *tim, uint8_t channel)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->pulse_inputs & (1U << (channel - 1))) {
        release_channel(info, state, channel);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_tim_pulse_rearm(TIM_TypeDef *tim, uint8_t channel)
{
    const timer_info_t *info = info_for(tim);
    if (!info || channel < 1 || channel > 4) return BSP_ERR_ARG;
    timer_state_t *state = &states[info - timers];
    uint8_t bit = 1U << (channel - 1U);
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (state->pulse_inputs & bit) {
        bsp_pin_t pin = channel_pin(info, state->route, channel);
        result = BSP_ERR_BUSY;
        if (!GPIO_ReadInputDataBit(pin.port, pin.pin)) {
            uint16_t flags = (bit | paired_mask(bit)) << 1;
            TIM_ClearFlag(tim, flags | (flags << 8));
            result = BSP_OK;
        }
    }
    bsp_irq_restore(level);
    return result;
}
