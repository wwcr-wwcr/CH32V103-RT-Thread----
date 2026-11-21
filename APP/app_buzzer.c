#include "app_buzzer.h"
#include "app_internal.h"
#include "private/app_output_pattern.h"

#define BUZZER_CHANNEL 3U
#define BUZZER_DEFAULT_HZ 2000U
#define BUZZER_MIN_HZ 100U
#define BUZZER_MAX_HZ 10000U
static app_device_t device;
static app_output_pattern_t sequence;
static uint8_t phase_locked;

static app_status_t write_duty(uint16_t duty)
{
    if (duty) {
        bsp_status_t rc = bsp_tim_pwm_restart(TIM2, BUZZER_CHANNEL, duty);
        if (rc != BSP_ERR_BUSY || phase_locked) return app_from_bsp(rc);
    }
    return app_from_bsp(bsp_tim_pwm_write_immediate(TIM2, BUZZER_CHANNEL, duty));
}

static void unlock_phase(void)
{
    if (phase_locked) {
        (void)bsp_tim_pwm_lock_phase(TIM2, BUZZER_CHANNEL, 0);
        phase_locked = 0;
    }
}

app_status_t app_buzzer_init(void)
{
    app_status_t result = app_device_enter(&device, 0);
    if (result != APP_OK) return result;
    result = app_from_bsp(bsp_tim_pwm_init_hz(TIM2, BUZZER_CHANNEL, BUZZER_DEFAULT_HZ, 0));
    if (result == APP_OK) app_output_pattern_init(&sequence, "buzz_seq", write_duty, unlock_phase);
    app_device_leave(&device, result == APP_OK);
    return result;
}

static app_status_t start(uint32_t hz, uint16_t duty, int reserve_phase)
{
    if (hz < BUZZER_MIN_HZ || hz > BUZZER_MAX_HZ) return APP_ERR_ARG;
    if (sequence.remaining) return APP_ERR_BUSY;
    if (reserve_phase) {
        app_status_t rc = app_from_bsp(bsp_tim_pwm_lock_phase(TIM2, BUZZER_CHANNEL, 1));
        if (rc != APP_OK) return rc;
        phase_locked = 1;
    }
    app_status_t rc = app_from_bsp(bsp_tim_pwm_set_frequency(TIM2, BUZZER_CHANNEL, hz, duty));
    if (rc != APP_OK) unlock_phase();
    return rc;
}

app_status_t app_buzzer_repeat(uint32_t frequency_hz, uint8_t duty_percent, uint32_t on_ms,
                               uint32_t gap_ms, uint32_t count)
{
    app_output_phase_t on = {.level = (uint16_t)duty_percent * 10U}, gap = {0};
    if (duty_percent > 100 || !count || !app_output_durations(on_ms, gap_ms, &on, &gap))
        return APP_ERR_ARG;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    /* 短于或等于一个周期的响段须能重置 CNT，否则可能整段处于低电平。 */
    int short_pulse = duty_percent && duty_percent < 100 && (uint64_t)on_ms * frequency_hz <= 1000U;
    result = start(frequency_hz, 0, short_pulse);
    if (result == APP_OK) result = app_output_pattern_start(&sequence, on, gap, count);
    app_device_leave(&device, 1);
    return result;
}
app_status_t app_buzzer_start(uint32_t frequency_hz)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = start(frequency_hz, 500, 0);
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_buzzer_stop(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = app_output_pattern_set(&sequence, 0);
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_buzzer_beep(uint32_t frequency_hz, uint16_t duration_ms)
{
    if (!duration_ms) return APP_ERR_ARG;
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = start(frequency_hz, 500, (uint64_t)duration_ms * frequency_hz <= 1000U);
    if (result == APP_OK) {
        rt_tick_t since = rt_tick_get();
        rt_tick_t wait = rt_tick_from_millisecond(duration_ms) + 1U;
        rt_tick_t elapsed = 0;
        rt_err_t sleep_result;
        do {
            sleep_result = rt_thread_delay(wait - elapsed);
            elapsed = rt_tick_get() - since;
        } while (sleep_result == RT_EOK && elapsed < wait);
        result = app_output_pattern_set(&sequence, 0);
        if (result == APP_OK && sleep_result != RT_EOK) result = APP_ERR_IO;
    }
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_buzzer_de_init(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    rt_base_t irq = rt_hw_interrupt_disable();
    result = app_from_bsp(bsp_tim_pwm_de_init(TIM2, BUZZER_CHANNEL));
    if (result == APP_OK) app_output_pattern_release(&sequence);
    rt_hw_interrupt_enable(irq);
    app_device_leave(&device, result != APP_OK);
    return result;
}
