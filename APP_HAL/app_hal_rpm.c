#include "app_hal_rpm.h"
#include "app_hal_monitor_private.h"
#include "app_encoder.h"

static app_hal_rpm_sample_t data;
static uint32_t counts_per_turn, last_read;
static uint8_t valid;

static app_status_t page(void)
{
    char text[6];
    monitor_text(valid ? data.rpm * data.direction : NAN, 0, text);
    return hal_page(HAL_RPM, text, APP_RGB_OFF);
}

app_status_t app_hal_rpm_init(uint32_t counts_per_rev)
{
    if (!counts_per_rev) return APP_ERR_ARG;
    app_status_t rc = hal_open(HAL_RPM, HAL_SEG | HAL_ENCODER);
    if (rc != APP_OK) return rc;
    counts_per_turn = counts_per_rev;
    last_read = hal_now_ms();
    data = (app_hal_rpm_sample_t){0};
    valid = 0;
    hal_schedule(HAL_RPM, app_hal_rpm_update);
    return APP_OK;
}

app_status_t app_hal_rpm_read(app_hal_rpm_sample_t *sample)
{
    if (!sample) return APP_ERR_ARG;
    if (!hal_active(HAL_RPM)) return APP_ERR_STATE;
    if (!valid) return APP_ERR_BUSY;
    *sample = data;
    return APP_OK;
}

app_status_t app_hal_rpm_show(void)
{
    if (!hal_active(HAL_RPM)) return APP_ERR_STATE;
    app_status_t rc = page();
    return rc == APP_OK ? hal_focus(HAL_RPM) : rc;
}

app_status_t app_hal_rpm_update(void)
{
    if (!hal_active(HAL_RPM)) return APP_ERR_STATE;
    uint32_t now = hal_now_ms(), elapsed = now - last_read;
    if (!elapsed || elapsed < APP_HAL_RPM_PERIOD_MS) return APP_OK;
    app_encoder_sample_t sample;
    app_status_t rc = app_encoder_read(&sample);
    if (rc != APP_OK) {
        valid = 0;
        (void)page();
        return rc;
    }
    data.direction = (sample.delta > 0) - (sample.delta < 0);
    data.rpm = fabsf((float)sample.delta) * 60000.0f / (counts_per_turn * (float)elapsed);
    data.position = sample.position;
    valid = 1;
    last_read = now;
    return page();
}

app_status_t app_hal_rpm_de_init(void)
{
    return hal_close(HAL_RPM);
}
