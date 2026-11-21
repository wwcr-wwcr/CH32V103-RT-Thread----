#ifndef APP_HAL_WAVE_PRIVATE_H
#define APP_HAL_WAVE_PRIVATE_H
#include "app_hal_wave_input.h"
#include "app_hal_internal.h"
#include <rtthread.h>
#if APP_HAL_WAVE_VREF_MV <= 0 || APP_HAL_WAVE_VREF_MV > 65535 || APP_HAL_WAVE_RATE_HZ < 2000 || APP_HAL_WAVE_RATE_HZ > 48000
#error Invalid default waveform reference voltage or sampling rate
#endif

/* Phase 0..1 starts with a positive-going midpoint crossing. */
static inline float wave_triangle(float phase)
{
    if (phase < 0.25f) return 4.0f * phase;
    return phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f;
}

/* Smooth approximation of sin(2*pi*phase); peak absolute error < 0.0011. */
static inline float wave_sine(float phase)
{
    float tri = wave_triangle(phase);
    float y = tri * (2.0f - (tri < 0 ? -tri : tri));
    return y * (0.775f + 0.225f * (y < 0 ? -y : y));
}

app_status_t hal_wave_send(const char *line, size_t length, app_hal_wave_write_t write, void *context);
extern uint8_t hal_wave_loopback_call;
static inline int wave_taken(void)
{
    return hal_active(HAL_WAVE_LOOPBACK) && !hal_wave_loopback_call;
}
#endif
