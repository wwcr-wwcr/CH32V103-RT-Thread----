#include "app_hal_proximity.h"
#include "app_hal_monitor_private.h"

static app_hal_proximity_state_t state;
static monitor_poll_t poll;
static uint16_t near_mm, far_mm;
static uint8_t count_page;

static app_status_t page(void)
{
    char text[6];
    float value = count_page ? (float)state.passages : state.valid ? (float)state.distance_mm : NAN;
    monitor_text(value, 0, text);
    return hal_page(HAL_PROXIMITY, text, state.valid ? APP_RGB_GREEN : APP_RGB_OFF);
}

app_status_t app_hal_proximity_configure(uint16_t near, uint16_t far)
{
    if (!near || far <= near) return APP_ERR_ARG;
    if (!hal_active(HAL_PROXIMITY)) return APP_ERR_STATE;
    near_mm = near;
    far_mm = far;
    poll.started = 0;
    return APP_OK;
}

app_status_t app_hal_proximity_init(void)
{
    app_status_t rc = hal_open(HAL_PROXIMITY, HAL_SENSOR_BIT(HAL_DISTANCE) | HAL_SEG | HAL_RGB | HAL_BUZZ);
    if (rc != APP_OK) return rc;
    rc = app_hal_proximity_configure(APP_HAL_PROX_NEAR_MM, APP_HAL_PROX_FAR_MM);
    if (rc != APP_OK) { (void)hal_close(HAL_PROXIMITY); return rc; }
    state = (app_hal_proximity_state_t){0};
    count_page = 0;
    hal_schedule(HAL_PROXIMITY, app_hal_proximity_update);
    return APP_OK;
}

app_status_t app_hal_proximity_reset_count(void)
{
    if (!hal_active(HAL_PROXIMITY)) return APP_ERR_STATE;
    state.passages = 0;
    return page();
}

app_status_t app_hal_proximity_get(app_hal_proximity_state_t *out)
{
    if (!out) return APP_ERR_ARG;
    if (!hal_active(HAL_PROXIMITY)) return APP_ERR_STATE;
    *out = state;
    return APP_OK;
}

app_status_t app_hal_proximity_show(uint8_t show_count)
{
    if (show_count > 1) return APP_ERR_ARG;
    if (!hal_active(HAL_PROXIMITY)) return APP_ERR_STATE;
    count_page = show_count;
    app_status_t rc = page();
    return rc == APP_OK ? hal_focus(HAL_PROXIMITY) : rc;
}

app_status_t app_hal_proximity_update(void)
{
    if (!hal_active(HAL_PROXIMITY)) return APP_ERR_STATE;
    if (!monitor_due(&poll, hal_now_ms(), APP_HAL_SENSOR_PERIOD_MS)) return APP_OK;
    float distance;
    app_status_t rc = hal_read(HAL_PROXIMITY, HAL_DISTANCE, &distance);
    if (rc == APP_OK && !(distance >= 0 && distance <= UINT16_MAX)) rc = APP_ERR_IO;
    state.valid = rc == APP_OK;
    if (state.valid) {
        state.distance_mm = (uint16_t)distance;
        if (!state.occupied && state.distance_mm <= near_mm) {
            state.occupied = 1;
            if (state.passages < UINT32_MAX) ++state.passages;
        } else if (state.distance_mm >= far_mm) state.occupied = 0;
    }
    app_status_t display_rc = page();
    app_status_t alert_rc;
    if (!state.valid || state.distance_mm >= far_mm)
        alert_rc = hal_alert_clear(HAL_PROXIMITY);
    else {
        /* Two fixed cadences leave an in-progress beep intact when the measurement jitters. */
        int near = state.distance_mm <= near_mm;
        alert_rc = hal_alert(HAL_PROXIMITY, APP_HAL_PRIORITY_NOTICE, NULL,
                             near ? APP_RGB_RED : APP_RGB_YELLOW, APP_HAL_ALERT_HZ,
                             APP_HAL_ALERT_ON_MS, near ? APP_HAL_PROX_GAP_NEAR_MS : APP_HAL_PROX_GAP_FAR_MS);
    }
    if (rc != APP_OK) return rc;
    return display_rc == APP_OK ? alert_rc : display_rc;
}

app_status_t app_hal_proximity_de_init(void)
{
    return hal_close(HAL_PROXIMITY);
}
