#include "app_hal_counter.h"
#include "app_hal_ui_private.h"

static app_hal_counter_config_t cfg;
static int32_t value;
static uint8_t started, notice;
static uint32_t notice_at;
static ui_repeat_t repeat;

static app_status_t render(void)
{
    char text[6];
    ui_fixed_text(value, 0, text);
    return hal_page(HAL_COUNTER, text, APP_RGB_OFF);
}
static app_status_t clear_notice(void)
{
    if (!notice) return APP_OK;
    app_status_t rc = hal_alert_clear(HAL_COUNTER);
    if (rc == APP_OK) notice = 0;
    return rc;
}
app_status_t app_hal_counter_init(void)
{
    app_status_t rc = hal_open(HAL_COUNTER, HAL_SEG | HAL_KEYS);
    if (rc == APP_OK) {
        started = notice = 0;
        hal_schedule(HAL_COUNTER, app_hal_counter_update);
    }
    return rc;
}
app_status_t app_hal_counter_de_init(void) { return hal_close(HAL_COUNTER); }

app_status_t app_hal_counter_start(const app_hal_counter_config_t *config)
{
    const app_hal_counter_config_t defaults = {
        .minimum = APP_HAL_COUNTER_DEFAULT_MIN, .maximum = APP_HAL_COUNTER_DEFAULT_MAX,
        .initial = APP_HAL_COUNTER_DEFAULT_MIN, .step = APP_HAL_COUNTER_DEFAULT_STEP,
        .target = APP_HAL_COUNTER_DEFAULT_TARGET, .wrap = 0, .notify_target = 1
    };
    if (!config) config = &defaults;
    if (!ui_fixed_fits(config->minimum, 0) || !ui_fixed_fits(config->maximum, 0) ||
        config->initial < config->minimum || config->initial > config->maximum || config->step <= 0 ||
        config->wrap > 1 || config->notify_target > 1 ||
        (config->notify_target && (config->target < config->minimum || config->target > config->maximum)))
        return APP_ERR_ARG;
    if (!hal_active(HAL_COUNTER)) return APP_ERR_STATE;
    app_status_t rc = config->notify_target ? hal_require(HAL_COUNTER, HAL_RGB | HAL_BUZZ) : APP_OK;
    if (rc == APP_OK) rc = hal_focus(HAL_COUNTER);
    if (rc == APP_OK) rc = clear_notice();
    if (rc != APP_OK) return rc;
    cfg = *config;
    value = cfg.initial;
    started = 1;
    repeat = (ui_repeat_t){0};
    return render();
}
app_status_t app_hal_counter_set_target(int32_t target)
{
    if (!hal_active(HAL_COUNTER) || !started) return APP_ERR_STATE;
    if (target < cfg.minimum || target > cfg.maximum) return APP_ERR_ARG;
    app_status_t rc = hal_require(HAL_COUNTER, HAL_RGB | HAL_BUZZ);
    if (rc == APP_OK) rc = clear_notice();
    if (rc == APP_OK) { cfg.target = target; cfg.notify_target = 1; }
    return rc;
}
app_status_t app_hal_counter_step(int8_t direction)
{
    if (direction != 1 && direction != -1) return APP_ERR_ARG;
    if (!hal_active(HAL_COUNTER) || !started) return APP_ERR_STATE;
    int32_t width = cfg.maximum - cfg.minimum + 1;
    int32_t steps = cfg.step;
    int32_t distance = cfg.notify_target ? (direction > 0 ? cfg.target - value : value - cfg.target) : 0;
    int64_t next = (int64_t)value + (int64_t)direction * steps;
    if (cfg.wrap) {
        int64_t offset = (next - cfg.minimum) % width;
        next = cfg.minimum + (offset < 0 ? offset + width : offset);
        if (cfg.notify_target && distance <= 0) distance += width;
    }
    if (next > cfg.maximum) next = cfg.maximum;
    if (next < cfg.minimum) next = cfg.minimum;
    value = (int32_t)next;
    if (cfg.notify_target && width > 1 && distance > 0 && distance <= steps) {
        app_status_t rc = hal_alert(HAL_COUNTER, APP_HAL_PRIORITY_NOTICE, NULL, APP_RGB_GREEN,
                                    APP_HAL_ALERT_HZ, APP_HAL_ALERT_ON_MS, APP_HAL_ALERT_GAP_MS);
        if (rc != APP_OK) return rc;
        notice = 1;
        notice_at = hal_now_ms();
    }
    return render();
}
app_status_t app_hal_counter_get(int32_t *number)
{
    if (!number) return APP_ERR_ARG;
    if (!hal_active(HAL_COUNTER) || !started) return APP_ERR_STATE;
    *number = value;
    return APP_OK;
}
app_status_t app_hal_counter_update(void)
{
    if (!hal_active(HAL_COUNTER)) return APP_ERR_STATE;
    if (!started) return APP_OK;
    if (notice && hal_now_ms() - notice_at >= APP_HAL_COUNTER_NOTICE_MS) {
        app_status_t rc = clear_notice();
        if (rc != APP_OK) return rc;
    }
    int delta = ui_key_delta(HAL_COUNTER, &repeat);
    if (hal_key_events(HAL_COUNTER, APP_KEY_3) & APP_KEY_EVENT_CLICK) {
        app_status_t rc = clear_notice();
        if (rc != APP_OK) return rc;
        value = cfg.initial;
        return render();
    }
    while (delta) {
        int sign = delta > 0 ? 1 : -1;
        app_status_t rc = app_hal_counter_step(sign);
        if (rc != APP_OK) return rc;
        delta -= sign;
    }
    return APP_OK;
}
