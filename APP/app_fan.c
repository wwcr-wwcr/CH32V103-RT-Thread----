#include "app_fan.h"
#include "app_internal.h"

#define FAN_PWM_HZ 1000U
#define FAN_CHANNEL 1U

static app_device_t fan;

app_status_t app_fan_init(void)
{
    app_status_t result = app_device_enter(&fan, 0);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_pwm_init_hz(TIM5, FAN_CHANNEL, FAN_PWM_HZ, 0);
    app_device_leave(&fan, status == BSP_OK);
    return app_from_bsp(status);
}

app_status_t app_fan_set_speed(uint8_t percent)
{
    if (percent > 100U) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&fan, 1);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_pwm_write(TIM5, FAN_CHANNEL, percent * 10U);
    app_device_leave(&fan, 1);
    return app_from_bsp(status);
}

app_status_t app_fan_de_init(void)
{
    app_status_t result = app_device_enter(&fan, 1);
    if (result != APP_OK) return result;

    /* 定时器关闭输出后，引脚由板上 R40（4.7 kΩ）下拉到关闭电平。 */
    bsp_status_t status = bsp_tim_pwm_de_init(TIM5, FAN_CHANNEL);
    app_device_leave(&fan, status != BSP_OK);
    return app_from_bsp(status);
}
