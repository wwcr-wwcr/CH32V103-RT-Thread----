#include "app_hal_fan.h"
#include "app_hal_monitor_private.h"
#include "app_fan.h"

static monitor_poll_t poll;
static float stop_temp, start_temp, full_temp;
static uint8_t minimum, power, running;

app_status_t app_hal_fan_configure(float stop_c, float start_c, float full_c, uint8_t min_percent)
{
    if (!isfinite(stop_c) || !isfinite(start_c) || !isfinite(full_c) ||
        !(stop_c < start_c && start_c < full_c) || !min_percent || min_percent > 100) return APP_ERR_ARG;
    if (!hal_active(HAL_FAN)) return APP_ERR_STATE;
    stop_temp = stop_c;
    start_temp = start_c;
    full_temp = full_c;
    minimum = min_percent;
    poll.started = 0;
    return APP_OK;
}

app_status_t app_hal_fan_init(void)
{
    app_status_t rc = hal_open(HAL_FAN, HAL_SENSOR_BIT(HAL_TEMP) | HAL_FAN_DEV);
    if (rc != APP_OK) return rc;
    rc = app_hal_fan_configure(APP_HAL_FAN_STOP_C, APP_HAL_FAN_START_C, APP_HAL_FAN_FULL_C,
                               APP_HAL_FAN_MIN_PERCENT);
    if (rc != APP_OK) { (void)hal_close(HAL_FAN); return rc; }
    power = running = 0;
    hal_schedule(HAL_FAN, app_hal_fan_update);
    return APP_OK;
}

app_status_t app_hal_fan_get_power(uint8_t *percent)
{
    if (!percent) return APP_ERR_ARG;
    if (!hal_active(HAL_FAN)) return APP_ERR_STATE;
    *percent = power;
    return APP_OK;
}

app_status_t app_hal_fan_update(void)
{
    if (!hal_active(HAL_FAN)) return APP_ERR_STATE;
    if (!monitor_due(&poll, hal_now_ms(), APP_HAL_SENSOR_PERIOD_MS)) return APP_OK;
    float temp;
    app_status_t rc = hal_read(HAL_FAN, HAL_TEMP, &temp);
    if (rc == APP_ERR_BUSY) return rc;
    if (rc == APP_OK && !isfinite(temp)) rc = APP_ERR_IO;
    uint8_t target = APP_HAL_FAN_FAULT_PERCENT;
    if (rc == APP_OK) {
        if (temp <= stop_temp) running = 0;
        else if (temp >= start_temp) running = 1;
        if (!running) target = 0;
        else if (temp >= full_temp) target = 100;
        else if (temp <= start_temp) target = minimum;
        else target = (uint8_t)(minimum + (temp - start_temp) / (full_temp - start_temp)
                                         * (100U - minimum) + 0.5f);
    }
    if (target != power) {
        app_status_t written = app_fan_set_speed(target);
        if (written != APP_OK) return written;
        power = target;
    }
    return rc;
}

app_status_t app_hal_fan_de_init(void)
{
    return hal_close(HAL_FAN);
}
