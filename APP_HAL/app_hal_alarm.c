#include "app_hal_alarm.h"
#include "app_hal_monitor_private.h"

static const hal_sensor_t sensors[APP_HAL_ALARM_SOURCE_COUNT] = {HAL_TEMP, HAL_LUX, HAL_SPL, HAL_DISTANCE};
static struct {
    monitor_rule_t rule;
    float value;
    uint8_t enabled, valid;
} channels[APP_HAL_ALARM_SOURCE_COUNT];
static monitor_poll_t poll;
static int displayed = -1;

static int valid_source(app_hal_alarm_source_t source)
{
    return (unsigned)source < APP_HAL_ALARM_SOURCE_COUNT;
}

/* Among simultaneous rules sound an unacknowledged one first. Equal rules use enum order. */
static app_status_t render_alarm(void)
{
    int selected = -1;
    for (int i = 0; i < APP_HAL_ALARM_SOURCE_COUNT; ++i) {
        if (!channels[i].enabled || !channels[i].rule.active) continue;
        if (selected < 0 || (channels[selected].rule.acknowledged && !channels[i].rule.acknowledged))
            selected = i;
    }
    if (selected < 0) {
        displayed = -1;
        return hal_alert_clear(HAL_ALARM);
    }
    char text[6];
    uint8_t decimals = selected == APP_HAL_ALARM_TEMPERATURE || selected == APP_HAL_ALARM_SOUND;
    monitor_text(channels[selected].valid ? channels[selected].value : NAN, decimals, text);
    app_status_t rc = hal_alert(HAL_ALARM, APP_HAL_PRIORITY_ALARM, text, APP_RGB_RED,
                                channels[selected].rule.acknowledged ? 0 : APP_HAL_ALERT_HZ,
                                APP_HAL_ALERT_ON_MS, APP_HAL_ALERT_GAP_MS);
    if (rc == APP_OK) displayed = selected;
    return rc;
}

app_status_t app_hal_alarm_init(void)
{
    app_status_t rc = hal_open(HAL_ALARM, HAL_RGB | HAL_SEG | HAL_BUZZ | HAL_KEYS);
    if (rc == APP_OK) {
        memset(channels, 0, sizeof(channels));
        displayed = -1;
        poll.started = 0;
        hal_schedule(HAL_ALARM, app_hal_alarm_update);
    }
    return rc;
}

app_status_t app_hal_alarm_configure(app_hal_alarm_source_t source, float trip, float reset,
                                     uint32_t confirm_ms)
{
    if (!valid_source(source) || !monitor_valid_rule(trip, reset, confirm_ms)) return APP_ERR_ARG;
    if ((source == APP_HAL_ALARM_LIGHT || source == APP_HAL_ALARM_DISTANCE) && (trip < 0 || reset < 0))
        return APP_ERR_ARG;
    if (!hal_active(HAL_ALARM)) return APP_ERR_STATE;
    app_status_t rc = hal_require(HAL_ALARM, HAL_SENSOR_BIT(sensors[source]));
    if (rc != APP_OK) return rc;
    channels[source].rule = (monitor_rule_t){.trip=trip, .reset=reset, .confirm_ms=confirm_ms};
    channels[source].enabled = 1;
    channels[source].valid = 0;
    poll.started = 0;
    return render_alarm();
}

app_status_t app_hal_alarm_enable_default(app_hal_alarm_source_t source)
{
    static const float trip[] = {APP_HAL_TEMP_TRIP_C, APP_HAL_LIGHT_TRIP_LUX,
                                 APP_HAL_NOISE_TRIP_DB, APP_HAL_PROX_NEAR_MM};
    static const float reset[] = {APP_HAL_TEMP_RESET_C, APP_HAL_LIGHT_RESET_LUX,
                                  APP_HAL_NOISE_RESET_DB, APP_HAL_PROX_FAR_MM};
    if (!valid_source(source)) return APP_ERR_ARG;
    return app_hal_alarm_configure(source, trip[source], reset[source], APP_HAL_ALARM_CONFIRM_MS);
}

app_status_t app_hal_alarm_disable(app_hal_alarm_source_t source)
{
    if (!valid_source(source)) return APP_ERR_ARG;
    if (!hal_active(HAL_ALARM) || !channels[source].enabled) return APP_ERR_STATE;
    app_status_t rc = hal_drop(HAL_ALARM, HAL_SENSOR_BIT(sensors[source]));
    if (rc != APP_OK) return rc;
    channels[source].enabled = 0;
    return render_alarm();
}

app_status_t app_hal_alarm_ack(app_hal_alarm_source_t source)
{
    if (!valid_source(source)) return APP_ERR_ARG;
    if (!hal_active(HAL_ALARM) || !channels[source].enabled || !channels[source].rule.active)
        return APP_ERR_STATE;
    channels[source].rule.acknowledged = 1;
    return render_alarm();
}

app_status_t app_hal_alarm_get(app_hal_alarm_source_t source, app_hal_alarm_state_t *state)
{
    if (!state || !valid_source(source)) return APP_ERR_ARG;
    if (!hal_active(HAL_ALARM) || !channels[source].enabled) return APP_ERR_STATE;
    *state = (app_hal_alarm_state_t){channels[source].value, channels[source].valid,
                                    channels[source].rule.active, channels[source].rule.acknowledged};
    return APP_OK;
}

app_status_t app_hal_alarm_update(void)
{
    if (!hal_active(HAL_ALARM)) return APP_ERR_STATE;
    /* 键事件属于上一轮实际显示的来源，先确认，再采样并挑选新告警。 */
    if ((hal_key_events(HAL_ALARM, APP_KEY_4) & APP_KEY_EVENT_CLICK) &&
        displayed >= 0 && channels[displayed].rule.active) {
        app_status_t rc = app_hal_alarm_ack((app_hal_alarm_source_t)displayed);
        if (rc != APP_OK) return rc;
    }
    uint32_t now = hal_now_ms();
    if (!monitor_due(&poll, now, APP_HAL_SENSOR_PERIOD_MS)) return APP_OK;
    app_status_t status = APP_OK;
    for (int i = 0; i < APP_HAL_ALARM_SOURCE_COUNT; ++i) {
        if (!channels[i].enabled) continue;
        float value;
        app_status_t rc = hal_read(HAL_ALARM, sensors[i], &value);
        if (rc == APP_OK && !isfinite(value)) rc = APP_ERR_IO;
        channels[i].valid = rc == APP_OK;
        if (rc == APP_OK) {
            channels[i].value = value;
            monitor_observe(&channels[i].rule, now, value);
        } else {
            channels[i].rule.pending = 0;
            if (status == APP_OK || status == APP_ERR_BUSY) status = rc;
        }
    }
    app_status_t rc = render_alarm();
    return rc == APP_OK ? status : rc;
}

app_status_t app_hal_alarm_de_init(void)
{
    return hal_close(HAL_ALARM);
}
