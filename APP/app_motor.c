#include "app_motor.h"
#include "app_internal.h"

#define MOTOR_PWM_HZ 10000U
#define MOTOR_INA_CHANNEL 1U
#define MOTOR_INB_CHANNEL 4U

static app_device_t motor;
static uint8_t running_channel;
static uint8_t owns_ina;

app_status_t app_motor_init(void)
{
    app_status_t result = app_device_enter(&motor, 0);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_pwm_init_hz(TIM1, MOTOR_INA_CHANNEL, MOTOR_PWM_HZ, 0);
    if (status == BSP_OK) {
        status = bsp_tim_pwm_init_hz(TIM1, MOTOR_INB_CHANNEL, MOTOR_PWM_HZ, 0);
        if (status == BSP_OK) owns_ina = 1;
        else (void)bsp_tim_pwm_de_init(TIM1, MOTOR_INA_CHANNEL);
    }
    if (status == BSP_OK) running_channel = 0;
    app_device_leave(&motor, status == BSP_OK);
    return app_from_bsp(status);
}

/* 写入比较值后要等更新事件才能关断实际输出。按 RT tick 向上取整，并补偿首个不完整 tick。 */
static bsp_status_t stop_output(void)
{
    if (!running_channel) return BSP_OK;
    bsp_status_t status = bsp_tim_pwm_write(TIM1, running_channel, 0);
    if (status == BSP_OK) {
        rt_tick_t since = rt_tick_get();
        rt_tick_t wait = rt_tick_from_millisecond(1) + 1U;
        rt_tick_t elapsed = 0;
        do {
            rt_thread_delay(wait - elapsed);
            elapsed = rt_tick_get() - since;
        } while (elapsed < wait);
        running_channel = 0;
    }
    return status;
}

app_status_t app_motor_set_speed(int16_t percent)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    if (percent < -100 || percent > 100) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&motor, 1);
    if (result != APP_OK) return result;

    uint8_t channel = 0;
    if (percent > 0) channel = MOTOR_INB_CHANNEL;
    if (percent < 0) channel = MOTOR_INA_CHANNEL;
    bsp_status_t status = BSP_OK;
    if (channel != running_channel) status = stop_output();
    if (status == BSP_OK && channel) {
        uint16_t magnitude = (uint16_t)(percent > 0 ? percent : -percent);
        status = bsp_tim_pwm_write(TIM1, channel, magnitude * 10U);
        if (status == BSP_OK) running_channel = channel;
    }
    app_device_leave(&motor, 1);
    return app_from_bsp(status);
}

app_status_t app_motor_de_init(void)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t result = app_device_enter(&motor, 1);
    if (result != APP_OK) return result;

    bsp_status_t status = stop_output();
    if (status == BSP_OK && owns_ina) {
        status = bsp_tim_pwm_de_init(TIM1, MOTOR_INA_CHANNEL);
        if (status == BSP_OK) owns_ina = 0;
    }
    if (status == BSP_OK) status = bsp_tim_pwm_de_init(TIM1, MOTOR_INB_CHANNEL);
    app_device_leave(&motor, status != BSP_OK);
    return app_from_bsp(status);
}
