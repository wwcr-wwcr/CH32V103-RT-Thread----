#include "led.h"
#include "gpio.h"
#include "tim.h"
#include <rtthread.h>
#include <rthw.h>

static uint32_t period_ticks;

static int init_pwm(void)
{
    int rc = tim_prepare(TIM10, 1000);
    if (rc != RT_EOK) return rc;

    rt_base_t irq = rt_hw_interrupt_disable();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap_TIM10, ENABLE);
    rt_hw_interrupt_enable(irq);

    /* PB3 红、PB4 绿、PB5 蓝，低电平点亮。先让三个输出处于灭灯电平。 */
    TIM_OCInitTypeDef pwm = {0};
    pwm.TIM_OCMode = TIM_OCMode_PWM1;
    pwm.TIM_OutputState = TIM_OutputState_Enable;
    pwm.TIM_OCPolarity = TIM_OCPolarity_Low;
    pwm.TIM_OCIdleState = TIM_OCIdleState_Set;
    TIM_OC1Init(TIM10, &pwm);
    TIM_OC2Init(TIM10, &pwm);
    TIM_OC3Init(TIM10, &pwm);
    TIM_OC1PreloadConfig(TIM10, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM10, TIM_OCPreload_Enable);
    TIM_OC3PreloadConfig(TIM10, TIM_OCPreload_Enable);
    TIM_CtrlPWMOutputs(TIM10, ENABLE);
    TIM_Cmd(TIM10, ENABLE);
    gpio_config(GPIOB, GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5, GPIO_Mode_AF_PP);
    period_ticks = TIM10->ATRLR + 1U;
    return RT_EOK;
}

static uint16_t compare_value(uint8_t component, unsigned duty_percent)
{
    return (period_ticks * component * duty_percent + 12750U) / 25500U;
}

int led_set(led_color_t color, unsigned duty_percent)
{
    if (color > 0xffffffU || duty_percent > 100) return -RT_EINVAL;
    rt_enter_critical();
    int rc = period_ticks ? RT_EOK : init_pwm();
    if (rc == RT_EOK) {
        /* 暂停影子寄存器更新，三路比较值在同一周期边界生效。 */
        TIM_UpdateDisableConfig(TIM10, ENABLE);
        TIM_SetCompare1(TIM10, compare_value((uint8_t)(color >> 16), duty_percent));
        TIM_SetCompare2(TIM10, compare_value((uint8_t)(color >> 8), duty_percent));
        TIM_SetCompare3(TIM10, compare_value((uint8_t)color, duty_percent));
        TIM_UpdateDisableConfig(TIM10, DISABLE);
    }
    rt_exit_critical();
    return rc;
}
