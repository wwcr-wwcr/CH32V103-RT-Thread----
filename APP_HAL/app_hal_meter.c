#include "app_hal_meter.h"
#include "app_hal_ui_private.h"
#include "app_mic.h"
#include <math.h>
#include <string.h>

static const hal_sensor_t sensors[] = {HAL_TEMP, HAL_LUX, HAL_DBFS, HAL_SPL, HAL_HZ};
static const app_rgb_color_t colors[] = {
    APP_RGB_RED, APP_RGB_YELLOW, APP_RGB_BLUE, APP_RGB_MAGENTA, APP_RGB_CYAN
};
static app_hal_meter_sample_t history[APP_HAL_METER_PAGES];
static float held_reading[APP_HAL_METER_PAGES];
static app_hal_meter_page_t selected;
static app_hal_meter_view_t view;
static uint8_t sampled, keep_spl;
static uint32_t last_read;

static app_status_t render(void)
{
    const app_hal_meter_sample_t *sample = &history[selected];
    int valid = view == APP_HAL_METER_LIVE ? (sample->held || sample->has_current) : sample->has_extrema;
    float value = view == APP_HAL_METER_MIN ? sample->minimum :
                  view == APP_HAL_METER_MAX ? sample->maximum :
                  sample->held ? held_reading[selected] : sample->current;
    app_rgb_color_t color = colors[selected];
    if (!valid || !isfinite(value)) return hal_page(HAL_METER, "----", color);
    unsigned decimals = selected == APP_HAL_METER_LIGHT || selected == APP_HAL_METER_FREQUENCY ? 0U : 1U;
    if (!decimals && value >= 9999.5f) {
        value *= 0.001f;
        decimals = 2;
        color = selected == APP_HAL_METER_LIGHT ? APP_RGB_GREEN : APP_RGB_WHITE;
    }
    /* Preserve as many decimals as fit without ever wrapping the display. */
    float scale = decimals == 2 ? 100.0f : decimals == 1 ? 10.0f : 1.0f;
    float rounded = 0;
    for (;;) {
        rounded = roundf(value * scale);
        if (rounded >= -999.0f && rounded <= 9999.0f) break;
        if (!decimals) return hal_page(HAL_METER, "----", color);
        --decimals;
        scale *= 0.1f;
    }
    char text[6];
    ui_fixed_text((int32_t)rounded, decimals, text);
    return hal_page(HAL_METER, text, color);
}

app_status_t app_hal_meter_init(void)
{
    app_status_t rc = hal_open(HAL_METER, HAL_SEG | HAL_RGB | HAL_KEYS);
    if (rc == APP_OK) {
        memset(history, 0, sizeof history);
        selected = APP_HAL_METER_PAGES;
        keep_spl = 0;
        hal_schedule(HAL_METER, app_hal_meter_update);
    }
    return rc;
}
app_status_t app_hal_meter_de_init(void) { return hal_close(HAL_METER); }

app_status_t app_hal_meter_select_page(app_hal_meter_page_t page)
{
    if ((unsigned)page >= APP_HAL_METER_PAGES) return APP_ERR_ARG;
    if (!hal_active(HAL_METER)) return APP_ERR_STATE;
    app_status_t rc = hal_require(HAL_METER, HAL_SENSOR_BIT(sensors[page]));
    if (rc != APP_OK) return rc;
    if (selected != page && selected != APP_HAL_METER_PAGES &&
        !(keep_spl && selected == APP_HAL_METER_SPL)) {
        rc = hal_drop(HAL_METER, HAL_SENSOR_BIT(sensors[selected]));
        if (rc != APP_OK) {
            if (!(keep_spl && page == APP_HAL_METER_SPL))
                (void)hal_drop(HAL_METER, HAL_SENSOR_BIT(sensors[page]));
            return rc;
        }
    }
    if (selected != page) {
        if (!history[page].held) history[page].has_current = 0;
        view = APP_HAL_METER_LIVE;
        sampled = 0;
    }
    selected = page;
    rc = hal_focus(HAL_METER);
    return rc == APP_OK ? render() : rc;
}
app_status_t app_hal_meter_calibrate_spl(float reference_db_spl)
{
    if (!isfinite(reference_db_spl)) return APP_ERR_ARG;
    if (!hal_active(HAL_METER)) return APP_ERR_STATE;
    app_status_t rc = hal_require(HAL_METER, HAL_SENSOR_BIT(HAL_SPL));
    if (rc != APP_OK) return rc;
    rc = app_mic_calibrate_spl(reference_db_spl);
    if (rc == APP_OK) {
        keep_spl = 1;
        history[APP_HAL_METER_SPL] = (app_hal_meter_sample_t){0};
        if (selected == APP_HAL_METER_SPL) sampled = 0;
    } else if (!keep_spl && selected != APP_HAL_METER_SPL) {
        (void)hal_drop(HAL_METER, HAL_SENSOR_BIT(HAL_SPL));
    }
    return rc;
}
app_status_t app_hal_meter_hold(uint8_t hold)
{
    if (hold > 1) return APP_ERR_ARG;
    if (!hal_active(HAL_METER) || selected == APP_HAL_METER_PAGES) return APP_ERR_STATE;
    if (hold && !history[selected].held) {
        if (!history[selected].has_current) return APP_ERR_NOT_FOUND;
        held_reading[selected] = history[selected].current;
    }
    history[selected].held = hold;
    if (!hold) sampled = 0;
    return render();
}
app_status_t app_hal_meter_show_peak(app_hal_meter_view_t type)
{
    if ((unsigned)type > APP_HAL_METER_MAX) return APP_ERR_ARG;
    if (!hal_active(HAL_METER) || selected == APP_HAL_METER_PAGES) return APP_ERR_STATE;
    view = type;
    return render();
}
app_status_t app_hal_meter_reset_peaks(void)
{
    if (!hal_active(HAL_METER) || selected == APP_HAL_METER_PAGES) return APP_ERR_STATE;
    app_hal_meter_sample_t *s = &history[selected];
    s->has_extrema = s->has_current;
    s->minimum = s->maximum = s->current;
    return render();
}
app_status_t app_hal_meter_get(app_hal_meter_page_t page, app_hal_meter_sample_t *sample)
{
    if ((unsigned)page >= APP_HAL_METER_PAGES || !sample) return APP_ERR_ARG;
    if (!hal_active(HAL_METER)) return APP_ERR_STATE;
    *sample = history[page];
    return APP_OK;
}
app_status_t app_hal_meter_update(void)
{
    if (!hal_active(HAL_METER)) return APP_ERR_STATE;
    if (selected == APP_HAL_METER_PAGES) return APP_OK;
    app_hal_meter_sample_t *s = &history[selected];
    if (hal_key_events(HAL_METER, APP_KEY_1) & APP_KEY_EVENT_CLICK)
        return app_hal_meter_select_page((selected + 1U) % APP_HAL_METER_PAGES);
    if (hal_key_events(HAL_METER, APP_KEY_2) & APP_KEY_EVENT_CLICK)
        return app_hal_meter_hold(!s->held);
    if (hal_key_events(HAL_METER, APP_KEY_3) & APP_KEY_EVENT_CLICK)
        return app_hal_meter_show_peak((view + 1U) % 3U);
    if (hal_key_events(HAL_METER, APP_KEY_4) & APP_KEY_EVENT_CLICK) return app_hal_meter_reset_peaks();
    uint32_t now = hal_now_ms();
    if (sampled && now - last_read < APP_HAL_METER_REFRESH_MS) return APP_OK;
    sampled = 1;
    last_read = now;
    float value;
    app_status_t rc = hal_read(HAL_METER, sensors[selected], &value);
    s->has_current = rc == APP_OK;
    if (rc == APP_OK) {
        s->current = value;
        if (!s->has_extrema || value < s->minimum) s->minimum = value;
        if (!s->has_extrema || value > s->maximum) s->maximum = value;
        s->has_extrema = 1;
    }
    app_status_t draw = render();
    return rc == APP_OK ? draw : rc;
}
