#include "tim.h"
#include <rthw.h>
#include <rtthread.h>

int tim_prepare(TIM_TypeDef *tim, uint32_t rate)
{
    uint32_t gate;
    if (tim == TIM2) gate = RCC_APB1Periph_TIM2;
    else if (tim == TIM3) gate = RCC_APB1Periph_TIM3;
    else if (tim == TIM6) gate = RCC_APB1Periph_TIM6;
    else if (tim == TIM7) gate = RCC_APB1Periph_TIM7;
    else if (tim == TIM10) gate = RCC_APB2Periph_TIM10;
    else return -RT_EINVAL;
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    uint32_t clock = tim == TIM10 ? clocks.PCLK2_Frequency : clocks.PCLK1_Frequency;
    if (clock != clocks.HCLK_Frequency) clock *= 2;
    if (!rate || rate > clock) return -RT_EINVAL;
    uint32_t ticks = (clock + rate / 2) / rate;
    /* TIM10 的全亮比较值为 ARR+1，给 100% 占空比留出一个计数。 */
    uint32_t max_period = tim == TIM10 ? UINT16_MAX : 65536U;
    uint32_t divider = (ticks + max_period - 1U) / max_period;
    uint32_t period = (ticks + divider / 2) / divider;
    rt_base_t irq = rt_hw_interrupt_disable();
    if (tim == TIM10) RCC_APB2PeriphClockCmd(gate, ENABLE);
    else RCC_APB1PeriphClockCmd(gate, ENABLE);
    TIM_DeInit(tim);
    rt_hw_interrupt_enable(irq);

    TIM_TimeBaseInitTypeDef base = {0};
    base.TIM_Prescaler = divider - 1U;
    base.TIM_Period = period - 1U;
    TIM_TimeBaseInit(tim, &base);
    TIM_ClearFlag(tim, TIM_FLAG_Update);
    return RT_EOK;
}
