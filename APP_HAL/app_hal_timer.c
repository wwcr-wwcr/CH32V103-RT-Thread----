#include "app_hal_timer.h"
#include "app_hal_ui_private.h"

/* Largest time representable as MM.SS. */
#define MAX_TIME_MS ((99U * 60U + 59U) * 1000U)
static app_hal_timer_status_t timer;
static uint32_t duration, last_tick, lap_tick, previous_split;
static uint8_t notify, show_lap;

static void advance(void)
{
    if (timer.state != APP_HAL_TIMER_RUNNING) return;
    uint32_t now = hal_now_ms(), delta = now - last_tick;
    uint32_t left = duration - timer.elapsed_ms;
    timer.elapsed_ms += delta < left ? delta : left;
    last_tick = now;
    timer.remaining_ms = timer.countdown ? duration - timer.elapsed_ms : 0;
    if (timer.elapsed_ms == duration) {
        timer.state = APP_HAL_TIMER_FINISHED;
        show_lap = 0;
        timer.alarm_active = timer.countdown && notify;
    }
}

static app_status_t render(void)
{
    uint32_t ms = timer.countdown ? timer.remaining_ms : show_lap ? timer.lap_ms : timer.elapsed_ms;
    unsigned seconds = timer.countdown ? (ms + 999U) / 1000U : ms / 1000U;
    char text[6];
    ui_pair_text(seconds / 60U, seconds % 60U, text);
    app_status_t rc = hal_page(HAL_TIMER, text, APP_RGB_OFF);
    if (rc == APP_OK && timer.alarm_active)
        rc = hal_alert(HAL_TIMER, APP_HAL_PRIORITY_TIMER, "00.00", APP_RGB_RED, APP_HAL_ALERT_HZ,
                       APP_HAL_ALERT_ON_MS, APP_HAL_ALERT_GAP_MS);
    return rc;
}

app_status_t app_hal_timer_init(void)
{
    app_status_t rc = hal_open(HAL_TIMER, HAL_SEG | HAL_KEYS);
    if (rc == APP_OK) {
        timer = (app_hal_timer_status_t){0};
        show_lap = 0;
        hal_schedule(HAL_TIMER, app_hal_timer_update);
    }
    return rc;
}
app_status_t app_hal_timer_de_init(void) { return hal_close(HAL_TIMER); }

static app_status_t begin(uint32_t ms, uint8_t countdown, uint8_t alarm)
{
    if (!hal_active(HAL_TIMER)) return APP_ERR_STATE;
    app_status_t rc = APP_OK;
    if (alarm) rc = hal_require(HAL_TIMER, HAL_RGB | HAL_BUZZ);
    if (rc == APP_OK) rc = hal_focus(HAL_TIMER);
    if (rc != APP_OK) return rc;
    if (timer.alarm_active && (rc = hal_alert_clear(HAL_TIMER)) != APP_OK) return rc;
    last_tick = hal_now_ms();
    duration = ms;
    previous_split = 0;
    notify = alarm;
    show_lap = 0;
    timer = (app_hal_timer_status_t){.state = APP_HAL_TIMER_RUNNING, .countdown = countdown,
                                     .remaining_ms = countdown ? ms : 0};
    return render();
}
app_status_t app_hal_timer_countup(void) { return begin(MAX_TIME_MS, 0, 0); }
app_status_t app_hal_timer_countdown(uint32_t ms, uint8_t alarm)
{
    if (!ms) ms = APP_HAL_COUNTDOWN_DEFAULT_MS;
    if (!ms || ms > MAX_TIME_MS || alarm > 1) return APP_ERR_ARG;
    return begin(ms, 1, alarm);
}

app_status_t app_hal_timer_pause(void)
{
    if (!hal_active(HAL_TIMER) || timer.state != APP_HAL_TIMER_RUNNING) return APP_ERR_STATE;
    advance();
    if (timer.state != APP_HAL_TIMER_FINISHED) timer.state = APP_HAL_TIMER_PAUSED;
    return render();
}
app_status_t app_hal_timer_resume(void)
{
    if (!hal_active(HAL_TIMER) || timer.state != APP_HAL_TIMER_PAUSED) return APP_ERR_STATE;
    last_tick = hal_now_ms();
    timer.state = APP_HAL_TIMER_RUNNING;
    return APP_OK;
}
app_status_t app_hal_timer_ack(void)
{
    if (!hal_active(HAL_TIMER) || !timer.alarm_active) return APP_ERR_STATE;
    app_status_t rc = hal_alert_clear(HAL_TIMER);
    if (rc == APP_OK) timer.alarm_active = 0;
    return rc;
}
app_status_t app_hal_timer_stop(void)
{
    if (!hal_active(HAL_TIMER)) return APP_ERR_STATE;
    if (timer.alarm_active) {
        app_status_t rc = app_hal_timer_ack();
        if (rc != APP_OK) return rc;
    }
    timer = (app_hal_timer_status_t){0};
    show_lap = 0;
    return render();
}
app_status_t app_hal_stopwatch_lap(uint32_t *lap_ms)
{
    if (!lap_ms) return APP_ERR_ARG;
    if (!hal_active(HAL_TIMER) || timer.countdown || timer.state != APP_HAL_TIMER_RUNNING)
        return APP_ERR_STATE;
    advance();
    timer.has_lap = show_lap = 1;
    timer.lap_ms = *lap_ms = timer.elapsed_ms - previous_split;
    previous_split = timer.elapsed_ms;
    lap_tick = hal_now_ms();
    return render();
}
app_status_t app_hal_timer_get(app_hal_timer_status_t *status)
{
    if (!status) return APP_ERR_ARG;
    if (!hal_active(HAL_TIMER)) return APP_ERR_STATE;
    advance();
    *status = timer;
    return APP_OK;
}
app_status_t app_hal_timer_update(void)
{
    if (!hal_active(HAL_TIMER)) return APP_ERR_STATE;
    advance();
    if (timer.state == APP_HAL_TIMER_IDLE) return APP_OK;
    if (show_lap && hal_now_ms() - lap_tick >= APP_HAL_LAP_HOLD_MS) show_lap = 0;
    if (timer.alarm_active && (hal_key_events(HAL_TIMER, APP_KEY_4) & APP_KEY_EVENT_CLICK))
        return app_hal_timer_ack();
    if (hal_key_events(HAL_TIMER, APP_KEY_3) & APP_KEY_EVENT_CLICK) return app_hal_timer_stop();
    if (hal_key_events(HAL_TIMER, APP_KEY_1) & APP_KEY_EVENT_CLICK) {
        if (timer.state == APP_HAL_TIMER_RUNNING) return app_hal_timer_pause();
        if (timer.state == APP_HAL_TIMER_PAUSED) return app_hal_timer_resume();
    }
    if (!timer.countdown && timer.state == APP_HAL_TIMER_RUNNING &&
        (hal_key_events(HAL_TIMER, APP_KEY_2) & APP_KEY_EVENT_CLICK)) {
        uint32_t elapsed;
        return app_hal_stopwatch_lap(&elapsed);
    }
    return render();
}
