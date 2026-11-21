#include "buzzer.h"
#include "gpio.h"
#include "tim.h"
#include <rtthread.h>

static struct rt_mutex mutex;
static int pwm_running;
static int buzzer_ipc_init(void) { return rt_mutex_init(&mutex, "buzzer", RT_IPC_FLAG_PRIO); }
INIT_COMPONENT_EXPORT(buzzer_ipc_init);

static void constant_level(int high)
{
    if (pwm_running) {
        TIM_Cmd(TIM2, DISABLE);
        pwm_running = 0;
    }
    gpio_config(GPIOA, GPIO_Pin_2, GPIO_Mode_Out_PP);
    GPIO_WriteBit(GPIOA, GPIO_Pin_2, high ? Bit_SET : Bit_RESET);
}

int buzzer_beep(uint8_t duty_percent, uint32_t hz, uint32_t duration_ms)
{
    if (duty_percent > 100 || duration_ms > INT32_MAX ||
        (duty_percent && (hz < 100 || hz > 10000))) return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    if (duty_percent == 0 || duty_percent == 100) {
        constant_level(duty_percent == 100);
    } else {
        rc = tim_prepare(TIM2, hz);
        if (rc != RT_EOK) {
            rt_mutex_release(&mutex);
            return rc;
        }
        TIM_OCInitTypeDef pwm = {0};
        pwm.TIM_OCMode = TIM_OCMode_PWM1;
        pwm.TIM_OutputState = TIM_OutputState_Enable;
        pwm.TIM_Pulse = ((TIM2->ATRLR + 1U) * duty_percent + 50U) / 100U;
        TIM_OC3Init(TIM2, &pwm);
        gpio_config(GPIOA, GPIO_Pin_2, GPIO_Mode_AF_PP);
        TIM_Cmd(TIM2, ENABLE);
        pwm_running = 1;
    }
    if (duration_ms && duty_percent) {
        rt_thread_mdelay(duration_ms);
        constant_level(0);
    }
    rt_mutex_release(&mutex);
    return RT_EOK;
}
