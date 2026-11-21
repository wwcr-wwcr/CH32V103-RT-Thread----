#include "app_hal_clock.h"
#include "app_hal_ui_private.h"
#include "app_rtc.h"

enum clock_mode { HIDDEN, SHOW, EDIT_HOUR, EDIT_MINUTE };
static enum clock_mode mode;
static app_rtc_time_t draft;
static uint32_t last_read;
static ui_repeat_t repeat;

static app_status_t display_time(const app_rtc_time_t *time)
{
    char text[6];
    ui_pair_text(time->hour, time->minute, text);
    return hal_page(HAL_CLOCK, text, mode == EDIT_HOUR ? APP_RGB_RED :
                                    mode == EDIT_MINUTE ? APP_RGB_BLUE : APP_RGB_OFF);
}
app_status_t app_hal_clock_init(void)
{
    app_status_t rc = hal_open(HAL_CLOCK, HAL_SEG | HAL_KEYS | HAL_RGB | HAL_RTC);
    if (rc == APP_OK) {
        mode = HIDDEN;
        hal_schedule(HAL_CLOCK, app_hal_clock_update);
    }
    return rc;
}
app_status_t app_hal_clock_de_init(void) { return hal_close(HAL_CLOCK); }

app_status_t app_hal_clock_show(void)
{
    if (!hal_active(HAL_CLOCK)) return APP_ERR_STATE;
    app_rtc_time_t time;
    app_status_t rc = app_rtc_get_time(&time);
    if (rc != APP_OK) return rc;
    rc = hal_focus(HAL_CLOCK);
    if (rc != APP_OK) return rc;
    mode = SHOW;
    last_read = hal_now_ms();
    return display_time(&time);
}
app_status_t app_hal_clock_edit_start(void)
{
    if (!hal_active(HAL_CLOCK)) return APP_ERR_STATE;
    if (mode == EDIT_HOUR || mode == EDIT_MINUTE) return APP_ERR_BUSY;
    app_status_t rc = app_rtc_get_time(&draft);
    if (rc != APP_OK) return rc;
    rc = hal_focus(HAL_CLOCK);
    if (rc != APP_OK) return rc;
    mode = EDIT_HOUR;
    repeat = (ui_repeat_t){0};
    return display_time(&draft);
}
app_status_t app_hal_clock_edit_finish(uint8_t save)
{
    if (save > 1) return APP_ERR_ARG;
    if (!hal_active(HAL_CLOCK) || (mode != EDIT_HOUR && mode != EDIT_MINUTE)) return APP_ERR_STATE;
    app_rtc_time_t time;
    app_status_t rc = app_rtc_get_time(&time); /* Keep the current date across midnight while editing. */
    if (rc != APP_OK) return rc;
    if (save) {
        time.hour = draft.hour;
        time.minute = draft.minute;
        time.second = 0;
        rc = app_rtc_set_time(&time);
        if (rc != APP_OK) return rc;
    }
    mode = SHOW;
    last_read = hal_now_ms();
    return display_time(&time);
}
app_status_t app_hal_clock_update(void)
{
    if (!hal_active(HAL_CLOCK)) return APP_ERR_STATE;
    if (mode == HIDDEN) return APP_OK;
    if (mode == SHOW) {
        if (hal_key_events(HAL_CLOCK, APP_KEY_3) & APP_KEY_EVENT_CLICK) return app_hal_clock_edit_start();
        uint32_t now = hal_now_ms();
        if (now - last_read < APP_HAL_CLOCK_REFRESH_MS) return APP_OK;
        app_rtc_time_t time;
        app_status_t rc = app_rtc_get_time(&time);
        last_read = now;
        if (rc != APP_OK) return rc;
        return display_time(&time);
    }
    int delta = ui_key_delta(HAL_CLOCK, &repeat);
    if (hal_key_events(HAL_CLOCK, APP_KEY_4) & APP_KEY_EVENT_CLICK) return app_hal_clock_edit_finish(0);
    if (hal_key_events(HAL_CLOCK, APP_KEY_3) & APP_KEY_EVENT_CLICK) {
        if (mode == EDIT_MINUTE) return app_hal_clock_edit_finish(1);
        mode = EDIT_MINUTE;
        repeat = (ui_repeat_t){0};
        return display_time(&draft);
    }
    if (!delta) return APP_OK;
    if (mode == EDIT_HOUR) draft.hour = (uint8_t)((draft.hour + delta + 24) % 24);
    else draft.minute = (uint8_t)((draft.minute + delta + 60) % 60);
    return display_time(&draft);
}
