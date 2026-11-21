#include "app_heater.h"
#include "app_internal.h"

#define HEATER_PWM_HZ 1000U
#define HEATER_CHANNEL 2U

static app_device_t heater;

app_status_t app_heater_init(void)
{
    app_status_t result = app_device_enter(&heater, 0);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_pwm_init_hz(TIM9, HEATER_CHANNEL, HEATER_PWM_HZ, 0);
    app_device_leave(&heater, status == BSP_OK);
    return app_from_bsp(status);
}

app_status_t app_heater_set_power(uint8_t percent)
{
    if (percent > 100U) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&heater, 1);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_pwm_write(TIM9, HEATER_CHANNEL, percent * 10U);
    app_device_leave(&heater, 1);
    return app_from_bsp(status);
}

app_status_t app_heater_de_init(void)
{
    app_status_t result = app_device_enter(&heater, 1);
    if (result != APP_OK) return result;

    /* 定时器关闭输出后，引脚由板上 R39（10 kΩ）下拉到关闭电平。 */
    bsp_status_t status = bsp_tim_pwm_de_init(TIM9, HEATER_CHANNEL);
    app_device_leave(&heater, status != BSP_OK);
    return app_from_bsp(status);
}
