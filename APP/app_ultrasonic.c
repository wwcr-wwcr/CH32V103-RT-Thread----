#include "app_ultrasonic.h"
#include "app_internal.h"

#define ECHO_CHANNEL      4U
#define TIMER_HZ          1000000U
#define TRIGGER_US        50U
#define TRIGGER_GAP_MS    50U
#define ECHO_TIMEOUT_MS   40U
#define ECHO_MIN_US       118U
#define ECHO_LIMIT_US     32000U

static app_device_t ultrasonic;
static uint8_t triggered;
static rt_tick_t last_trigger;

static bsp_status_t start_timer(void)
{
    RCC_ClocksTypeDef clocks;
    bsp_status_t status = bsp_rcc_read(&clocks);
    if (status != BSP_OK) return status;
    uint32_t hz = clocks.PCLK2_Frequency;
    if (hz != clocks.HCLK_Frequency) hz *= 2U;
    if (!hz || hz % TIMER_HZ || hz / TIMER_HZ > 65536U) return BSP_ERR_UNSUPPORTED;
    return bsp_tim_init(TIM10, (uint16_t)(hz / TIMER_HZ - 1U), UINT16_MAX);
}

app_status_t app_ultrasonic_init(void)
{
    app_status_t result = app_device_enter(&ultrasonic, 0);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_gpio_init(BSP_PIN_DIST_TRIG, GPIO_Mode_Out_PP, 0);
    if (status == BSP_OK) {
        status = start_timer();
        if (status == BSP_OK) {
            status = bsp_tim_pulse_init(TIM10, ECHO_CHANNEL, 0, 0);
            if (status != BSP_OK) bsp_tim_de_init(TIM10);
        }
        if (status != BSP_OK) bsp_gpio_de_init(BSP_PIN_DIST_TRIG);
    }
    app_device_leave(&ultrasonic, status == BSP_OK);
    return app_from_bsp(status);
}

static bsp_status_t trigger(void)
{
    bsp_status_t status = bsp_tim_pulse_rearm(TIM10, ECHO_CHANNEL);
    if (status != BSP_OK) return status;
    status = bsp_gpio_write(BSP_PIN_DIST_TRIG, 1);
    if (status != BSP_OK) return status;

    uint16_t start, now;
    rt_tick_t tick = rt_tick_get();
    status = bsp_tim_counter(TIM10, &start);
    while (status == BSP_OK) {
        status = bsp_tim_counter(TIM10, &now);
        if (status != BSP_OK || (uint16_t)(now - start) >= TRIGGER_US) break;
        if ((rt_tick_t)(rt_tick_get() - tick) >= rt_tick_from_millisecond(2) + 1U)
            status = BSP_ERR_TIMEOUT;
    }
    bsp_status_t off = bsp_gpio_write(BSP_PIN_DIST_TRIG, 0);
    last_trigger = rt_tick_get();
    triggered = 1;
    return off != BSP_OK ? off : status;
}

app_status_t app_ultrasonic_read_mm(uint16_t *distance_mm)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    if (!distance_mm) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&ultrasonic, 1);
    if (result != APP_OK) return result;

    rt_tick_t gap = rt_tick_from_millisecond(TRIGGER_GAP_MS) + 1U;
    rt_tick_t elapsed = rt_tick_get() - last_trigger;
    while (triggered && elapsed < gap) {
        rt_thread_delay(gap - elapsed);
        elapsed = rt_tick_get() - last_trigger;
    }
    uint16_t width;
    bsp_status_t status = trigger();
    if (status == BSP_OK) {
        rt_tick_t timeout = rt_tick_from_millisecond(ECHO_TIMEOUT_MS) + 1U;
        do {
            if ((rt_tick_t)(rt_tick_get() - last_trigger) >= timeout) {
                status = BSP_ERR_TIMEOUT;
                break;
            }
            status = bsp_tim_pulse_read(TIM10, ECHO_CHANNEL, &width);
            if (status != BSP_ERR_BUSY) break;
            rt_thread_delay(1);
        } while (1);
    }
    if (status == BSP_OK) {
        if (width < ECHO_MIN_US || width >= ECHO_LIMIT_US) status = BSP_ERR_IO;
        else *distance_mm = (uint16_t)(((uint32_t)width * 340U + 1000U) / 2000U);
    }
    app_device_leave(&ultrasonic, 1);
    return app_from_bsp(status);
}

app_status_t app_ultrasonic_de_init(void)
{
    app_status_t result = app_device_enter(&ultrasonic, 1);
    if (result != APP_OK) return result;
    bsp_status_t timer = bsp_tim_de_init(TIM10);
    bsp_status_t pin = bsp_gpio_de_init(BSP_PIN_DIST_TRIG);
    app_device_leave(&ultrasonic, 0);
    return app_from_bsp(timer != BSP_OK ? timer : pin);
}
