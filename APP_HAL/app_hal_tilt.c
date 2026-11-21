#include "app_hal_tilt.h"
#include "app_hal_monitor_private.h"
#include "app_imu.h"

static monitor_poll_t poll;
static monitor_rule_t rule;
static app_hal_tilt_state_t state;

static app_status_t page(void)
{
    char text[6];
    monitor_text(state.valid ? state.degrees : NAN, 1, text);
    return hal_page(HAL_TILT, text, APP_RGB_OFF);
}

app_status_t app_hal_tilt_configure(float trip_deg, float reset_deg, uint32_t confirm_ms)
{
    if (!monitor_valid_rule(trip_deg, reset_deg, confirm_ms) ||
        reset_deg < 0 || trip_deg > 180 || trip_deg <= reset_deg) return APP_ERR_ARG;
    if (!hal_active(HAL_TILT)) return APP_ERR_STATE;
    rule = (monitor_rule_t){.trip=trip_deg, .reset=reset_deg, .confirm_ms=confirm_ms};
    state.alarm = 0;
    poll.started = 0;
    return hal_alert_clear(HAL_TILT);
}

app_status_t app_hal_tilt_init(void)
{
    app_status_t rc = hal_open(HAL_TILT, HAL_IMU | HAL_SEG | HAL_RGB | HAL_BUZZ);
    if (rc != APP_OK) return rc;
    state = (app_hal_tilt_state_t){0};
    rc = app_hal_tilt_configure(APP_HAL_TILT_TRIP_DEG, APP_HAL_TILT_RESET_DEG, APP_HAL_ALARM_CONFIRM_MS);
    if (rc != APP_OK) { (void)hal_close(HAL_TILT); return rc; }
    hal_schedule(HAL_TILT, app_hal_tilt_update);
    return APP_OK;
}

app_status_t app_hal_tilt_get(app_hal_tilt_state_t *out)
{
    if (!out) return APP_ERR_ARG;
    if (!hal_active(HAL_TILT)) return APP_ERR_STATE;
    *out = state;
    return APP_OK;
}

app_status_t app_hal_tilt_show(void)
{
    if (!hal_active(HAL_TILT)) return APP_ERR_STATE;
    app_status_t rc = page();
    return rc == APP_OK ? hal_focus(HAL_TILT) : rc;
}

app_status_t app_hal_tilt_update(void)
{
    if (!hal_active(HAL_TILT)) return APP_ERR_STATE;
    uint32_t now = hal_now_ms();
    if (!monitor_due(&poll, now, APP_HAL_SENSOR_PERIOD_MS)) return APP_OK;
    app_imu_sample_t s;
    app_status_t rc = app_imu_read(&s);
    if (rc == APP_OK) {
        float x = s.accel_g[0], y = s.accel_g[1], z = s.accel_g[2];
        float horizontal = x*x + y*y, norm = sqrtf(horizontal + z*z);
        float gx = s.gyro_dps[0], gy = s.gyro_dps[1], gz = s.gyro_dps[2];
        float spin_sq = gx*gx + gy*gy + gz*gz;
        if (!isfinite(norm) || !isfinite(spin_sq)) rc = APP_ERR_IO;
        else if (fabsf(norm - 1.0f) > APP_HAL_TILT_G_TOLERANCE ||
                 spin_sq > APP_HAL_TILT_STILL_DPS * APP_HAL_TILT_STILL_DPS) rc = APP_ERR_BUSY;
        else {
            state.degrees = atan2f(sqrtf(horizontal), z) * 57.29577951f;
            monitor_observe(&rule, now, state.degrees);
        }
    }
    state.valid = rc == APP_OK;
    if (!state.valid) rule.pending = 0;
    state.alarm = rule.active; /* 运动或读取失败时保留已触发的告警，直到可靠测得恢复角度。 */
    app_status_t page_rc = page();
    char text[6];
    monitor_text(state.valid ? state.degrees : NAN, 1, text);
    app_status_t alert_rc = state.alarm ?
        hal_alert(HAL_TILT, APP_HAL_PRIORITY_ALARM, text, APP_RGB_RED,
                  APP_HAL_ALERT_HZ, APP_HAL_ALERT_ON_MS, APP_HAL_ALERT_GAP_MS) :
        hal_alert_clear(HAL_TILT);
    if (rc != APP_OK) return rc;
    return page_rc == APP_OK ? alert_rc : page_rc;
}

app_status_t app_hal_tilt_de_init(void)
{
    return hal_close(HAL_TILT);
}
