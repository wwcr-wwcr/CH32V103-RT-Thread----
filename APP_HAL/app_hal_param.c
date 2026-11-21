#include "app_hal_param.h"
#include "app_hal_ui_private.h"

static app_hal_param_range_t limits;
static int32_t original, value;
static app_hal_param_state_t state;
static ui_repeat_t repeat;

static app_status_t render(void)
{
    char text[6];
    ui_fixed_text(value, limits.decimals, text);
    return hal_page(HAL_PARAM, text, APP_RGB_OFF);
}

app_status_t app_hal_param_init(void)
{
    app_status_t rc = hal_open(HAL_PARAM, HAL_SEG | HAL_KEYS);
    if (rc == APP_OK) {
        state = APP_HAL_PARAM_IDLE;
        hal_schedule(HAL_PARAM, app_hal_param_update);
    }
    return rc;
}
app_status_t app_hal_param_de_init(void) { return hal_close(HAL_PARAM); }

app_status_t app_hal_param_edit_start(int32_t initial, const app_hal_param_range_t *range)
{
    if (!range || range->step <= 0 || range->minimum > initial || range->maximum < initial ||
        !ui_fixed_fits(range->minimum, range->decimals) || !ui_fixed_fits(range->maximum, range->decimals))
        return APP_ERR_ARG;
    if (!hal_active(HAL_PARAM)) return APP_ERR_STATE;
    if (state == APP_HAL_PARAM_EDITING) return APP_ERR_BUSY;
    app_status_t rc = hal_focus(HAL_PARAM);
    if (rc != APP_OK) return rc;
    limits = *range;
    original = value = initial;
    state = APP_HAL_PARAM_EDITING;
    repeat = (ui_repeat_t){0};
    return render();
}

app_status_t app_hal_param_edit_finish(uint8_t save)
{
    if (save > 1) return APP_ERR_ARG;
    if (!hal_active(HAL_PARAM) || state != APP_HAL_PARAM_EDITING) return APP_ERR_STATE;
    app_status_t rc = hal_blur(HAL_PARAM);
    if (rc == APP_OK) {
        if (!save) value = original;
        state = save ? APP_HAL_PARAM_SAVED : APP_HAL_PARAM_CANCELLED;
    }
    return rc;
}

app_status_t app_hal_param_get(app_hal_param_state_t *result, int32_t *number)
{
    if (!result || !number) return APP_ERR_ARG;
    if (!hal_active(HAL_PARAM) || state == APP_HAL_PARAM_IDLE) return APP_ERR_STATE;
    *result = state;
    *number = value;
    return APP_OK;
}

app_status_t app_hal_param_update(void)
{
    if (!hal_active(HAL_PARAM)) return APP_ERR_STATE;
    if (state != APP_HAL_PARAM_EDITING) return APP_OK;
    int delta = ui_key_delta(HAL_PARAM, &repeat);
    if (hal_key_events(HAL_PARAM, APP_KEY_4) & APP_KEY_EVENT_CLICK) return app_hal_param_edit_finish(0);
    if (hal_key_events(HAL_PARAM, APP_KEY_3) & APP_KEY_EVENT_CLICK) return app_hal_param_edit_finish(1);
    if (!delta) return APP_OK;
    int64_t next = (int64_t)value + (int64_t)delta * limits.step;
    value = next < limits.minimum ? limits.minimum : next > limits.maximum ? limits.maximum : (int32_t)next;
    return render();
}
