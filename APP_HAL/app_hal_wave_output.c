#include "app_hal_wave_output.h"
#include "app_hal_wave_private.h"
#include "app_analog_output.h"

#if APP_HAL_WAVE_OUTPUT_SAMPLES < 8 || APP_HAL_WAVE_OUTPUT_SAMPLES % 4 || APP_HAL_WAVE_DAC_CAPACITY <= APP_HAL_WAVE_OUTPUT_SAMPLES || APP_HAL_WAVE_DAC_CAPACITY > 65535
#error Invalid DAC waveform table dimensions
#endif
#if APP_HAL_WAVE_FREQUENCY_HZ <= 0 || APP_HAL_WAVE_FREQUENCY_HZ > (100000U / APP_HAL_WAVE_OUTPUT_SAMPLES) || APP_HAL_WAVE_OFFSET_MV > APP_HAL_WAVE_VREF_MV || APP_HAL_WAVE_AMPLITUDE_MVPP > 2U * APP_HAL_WAVE_OFFSET_MV || APP_HAL_WAVE_AMPLITUDE_MVPP > 2U * (APP_HAL_WAVE_VREF_MV - APP_HAL_WAVE_OFFSET_MV)
#error Invalid default DAC signal frequency, offset or amplitude
#endif
static uint16_t table[APP_HAL_WAVE_DAC_CAPACITY], reference_mv;

static app_status_t refresh_output(void)
{
    uint8_t running = 0;
    app_status_t rc = app_analog_output_running(&running);
    if (rc == APP_OK && !running) hal_schedule(HAL_WAVE_OUTPUT, NULL);
    return rc;
}

app_status_t app_hal_wave_output_init(uint8_t dac_channel, uint16_t vref_mv)
{
    app_status_t rc = hal_open(HAL_WAVE_OUTPUT, 0);
    if (rc != APP_OK) return rc;
    rc = app_analog_output_init(dac_channel);
    if (rc != APP_OK) { (void)hal_close(HAL_WAVE_OUTPUT); return rc; }
    reference_mv = vref_mv ? vref_mv : APP_HAL_WAVE_VREF_MV;
    return APP_OK;
}

app_status_t app_hal_wave_output_de_init(void)
{
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_OUTPUT)) return APP_ERR_STATE;
    app_status_t rc = app_analog_output_de_init();
    return rc == APP_OK ? hal_close(HAL_WAVE_OUTPUT) : rc;
}

static app_status_t generate(const app_hal_wave_signal_t *signal, uint32_t cycles, float *actual_hz)
{
    const app_hal_wave_signal_t defaults = {APP_HAL_WAVE_SINE, APP_HAL_WAVE_FREQUENCY_HZ,
                                            APP_HAL_WAVE_AMPLITUDE_MVPP, APP_HAL_WAVE_OFFSET_MV};
    if (!signal) signal = &defaults;
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_OUTPUT)) return APP_ERR_STATE;
    const unsigned n = APP_HAL_WAVE_OUTPUT_SAMPLES;
    if (signal->kind < APP_HAL_WAVE_SINE || signal->kind > APP_HAL_WAVE_SQUARE ||
        !signal->frequency_hz || signal->frequency_hz > 100000U / n ||
        signal->amplitude_mvpp > 2U * signal->offset_mv ||
        2U * signal->offset_mv + signal->amplitude_mvpp > 2U * reference_mv ||
        cycles > (APP_HAL_WAVE_DAC_CAPACITY - 1U) / n) return APP_ERR_ARG;
    uint8_t running;
    app_status_t rc = app_analog_output_running(&running);
    if (rc != APP_OK || running) return rc == APP_OK ? APP_ERR_BUSY : rc;
    for (unsigned i = 0; i < n; ++i) {
        float phase = (float)i / n;
        float value = signal->kind == APP_HAL_WAVE_SINE ? wave_sine(phase) : wave_triangle(phase);
        if (signal->kind == APP_HAL_WAVE_SQUARE) value = i < n / 2U ? 1.0f : -1.0f;
        table[i] = (uint16_t)((signal->offset_mv + 0.5f * signal->amplitude_mvpp * value) *
                              (4095.0f / reference_mv) + 0.5f);
    }
    uint16_t idle = (uint16_t)(((uint32_t)signal->offset_mv * 4095U + reference_mv / 2U) / reference_mv);
    unsigned length = n;
    if (cycles) {
        length = cycles * n;
        for (unsigned i = n; i < length; ++i) table[i] = table[i % n];
        table[length++] = idle;
    }
    float sample_hz;
    rc = app_analog_output_play(table, (uint16_t)length, signal->frequency_hz * n, !cycles, idle, &sample_hz);
    if (rc == APP_OK) {
        if (actual_hz) *actual_hz = sample_hz / n;
        hal_schedule(HAL_WAVE_OUTPUT, refresh_output);
    }
    return rc;
}

app_status_t app_hal_wave_output_start(const app_hal_wave_signal_t *signal, float *actual_hz)
{
    return generate(signal, 0, actual_hz);
}
app_status_t app_hal_wave_output_burst(const app_hal_wave_signal_t *signal, uint32_t cycles, float *actual_hz)
{
    return cycles ? generate(signal, cycles, actual_hz) : APP_ERR_ARG;
}
app_status_t app_hal_wave_output_running(uint8_t *running)
{
    if (wave_taken()) return APP_ERR_BUSY;
    return hal_active(HAL_WAVE_OUTPUT) ? app_analog_output_running(running) : APP_ERR_STATE;
}
app_status_t app_hal_wave_output_stop(void)
{
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_OUTPUT)) return APP_ERR_STATE;
    app_status_t rc = app_analog_output_stop();
    if (rc == APP_OK) hal_schedule(HAL_WAVE_OUTPUT, NULL);
    return rc;
}
