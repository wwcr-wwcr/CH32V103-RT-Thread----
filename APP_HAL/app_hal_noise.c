#include "app_hal_noise.h"
#include "app_hal_monitor_private.h"

static app_hal_noise_state_t state;
static monitor_poll_t poll;
static float threshold;
static uint32_t last_sample;
static uint8_t above;

app_status_t app_hal_noise_reset(void)
{
    if (!hal_active(HAL_NOISE)) return APP_ERR_STATE;
    state = (app_hal_noise_state_t){0};
    above = poll.started = 0;
    return APP_OK;
}

app_status_t app_hal_noise_set_threshold(float threshold_db_spl)
{
    if (!isfinite(threshold_db_spl)) return APP_ERR_ARG;
    if (!hal_active(HAL_NOISE)) return APP_ERR_STATE;
    threshold = threshold_db_spl;
    return app_hal_noise_reset();
}

app_status_t app_hal_noise_init(void)
{
    app_status_t rc = hal_open(HAL_NOISE, HAL_SENSOR_BIT(HAL_SPL));
    if (rc != APP_OK) return rc;
    rc = app_hal_noise_set_threshold(APP_HAL_NOISE_TRIP_DB);
    if (rc != APP_OK) { (void)hal_close(HAL_NOISE); return rc; }
    hal_schedule(HAL_NOISE, app_hal_noise_update);
    return APP_OK;
}

app_status_t app_hal_noise_get(app_hal_noise_state_t *out)
{
    if (!out) return APP_ERR_ARG;
    if (!hal_active(HAL_NOISE)) return APP_ERR_STATE;
    *out = state;
    return APP_OK;
}

app_status_t app_hal_noise_update(void)
{
    if (!hal_active(HAL_NOISE)) return APP_ERR_STATE;
    uint32_t now = hal_now_ms();
    if (!monitor_due(&poll, now, APP_HAL_SENSOR_PERIOD_MS)) return APP_OK;
    float db;
    app_status_t rc = hal_read(HAL_NOISE, HAL_SPL, &db);
    if (rc == APP_OK && !isfinite(db)) rc = APP_ERR_IO;
    if (rc != APP_OK) {
        state.valid = above = 0;
        return rc;
    }
    uint32_t elapsed = now - last_sample;
    uint8_t over = db >= threshold;
    if (above && over && elapsed <= APP_HAL_SENSOR_MAX_GAP_MS) state.exposure_ms += elapsed;
    if (!state.has_peak || db > state.peak_db_spl) state.peak_db_spl = db;
    state.current_db_spl = db;
    state.valid = state.has_peak = 1;
    last_sample = now;
    above = over;
    return APP_OK;
}

app_status_t app_hal_noise_de_init(void)
{
    return hal_close(HAL_NOISE);
}
