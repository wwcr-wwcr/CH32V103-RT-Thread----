#include "app_hal_wave_loopback.h"
#include "app_hal_wave_private.h"

static uint8_t owns_input, owns_output, testing;

static app_status_t release(void)
{
    app_status_t rc = APP_OK;
    hal_wave_loopback_call = 1;
    if (owns_output) {
        rc = app_hal_wave_output_de_init();
        if (rc == APP_OK) owns_output = 0;
    }
    if (owns_input) {
        app_status_t ri = app_hal_wave_input_de_init();
        if (ri == APP_OK) owns_input = 0;
        if (rc == APP_OK) rc = ri;
    }
    hal_wave_loopback_call = 0;
    if (rc == APP_OK) rc = hal_close(HAL_WAVE_LOOPBACK);
    return rc;
}

app_status_t app_hal_wave_loopback_init(uint8_t dac, uint8_t adc, uint8_t channel, uint32_t rate_hz,
                                        uint16_t vref_mv)
{
    app_status_t rc = hal_open(HAL_WAVE_LOOPBACK, 0);
    if (rc != APP_OK) return rc;
    rc = app_hal_wave_output_init(dac, vref_mv);
    if (rc == APP_OK) {
        owns_output = 1;
        rc = app_hal_wave_input_init(adc, channel, rate_hz, vref_mv);
        if (rc == APP_OK) owns_input = 1;
    }
    if (rc != APP_OK) (void)release();
    return rc;
}

app_status_t app_hal_wave_loopback_de_init(void)
{
    if (!hal_active(HAL_WAVE_LOOPBACK)) return APP_ERR_STATE;
    if (testing) return APP_ERR_BUSY;
    return release();
}

app_status_t app_hal_wave_loopback_test(const app_hal_wave_signal_t *signal, app_hal_wave_loopback_result_t *out,
                                        app_hal_wave_write_t write, void *context)
{
    if (!out) return APP_ERR_ARG;
    if (!hal_active(HAL_WAVE_LOOPBACK) || !owns_input || !owns_output) return APP_ERR_STATE;
    if (testing) return APP_ERR_BUSY;
    app_hal_wave_loopback_result_t result;
    testing = hal_wave_loopback_call = 1;
    app_status_t rc = app_hal_wave_output_start(signal, &result.generated_hz);
    if (rc == APP_OK) {
        /* Allow the first DAC sample and the analog output to settle. A second
         * acquisition begins with steady signal even for very low rates. */
        rc = app_hal_wave_measure(&result.measured);
        if (rc == APP_OK) rc = app_hal_wave_measure(&result.measured);
        app_status_t stop = app_hal_wave_output_stop();
        if (stop != APP_OK) rc = stop;
    }
    hal_wave_loopback_call = 0;
    if (rc == APP_OK && !result.measured.periodic) rc = APP_ERR_NOT_FOUND;
    if (rc == APP_OK) {
        result.frequency_error_hz = result.measured.frequency_hz - result.generated_hz;
        result.amplitude_error_mv = result.measured.peak_to_peak_mv -
                                    (signal ? signal->amplitude_mvpp : APP_HAL_WAVE_AMPLITUDE_MVPP);
        char line[64];
        int len = rt_snprintf(line, sizeof line, "loopback,%d,%d\n",
                              (int)(result.frequency_error_hz + (result.frequency_error_hz < 0 ? -0.5f : 0.5f)),
                              (int)(result.amplitude_error_mv + (result.amplitude_error_mv < 0 ? -0.5f : 0.5f)));
        rc = hal_wave_send(line, (size_t)len, write, context);
        if (rc == APP_OK) *out = result;
    }
    testing = 0;
    return rc;
}
