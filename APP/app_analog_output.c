#include "app_analog_output.h"
#include "app_internal.h"

static app_device_t dev;
static uint8_t dac, playing;
static uint16_t idle;

app_status_t app_analog_output_init(uint8_t channel)
{
    app_status_t rc = app_device_enter(&dev, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_dac_init(channel));
    if (rc == APP_OK) { dac = channel; playing = 0; }
    app_device_leave(&dev, rc == APP_OK);
    return rc;
}

static app_status_t stop(void)
{
    if (!playing) return APP_OK;
    app_status_t rc = app_from_bsp(bsp_dac_playback_stop(dac, idle));
    if (rc == APP_OK) playing = 0;
    return rc;
}

app_status_t app_analog_output_play(const uint16_t *table, uint16_t count, uint32_t sample_hz,
                                    uint8_t circular, uint16_t idle_code, float *actual_hz)
{
    if (idle_code > 4095U) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    rc = playing ? APP_ERR_BUSY :
         app_from_bsp(bsp_dac_playback_start(dac, table, count, sample_hz, circular, actual_hz));
    if (rc == APP_OK) { playing = 1; idle = idle_code; }
    app_device_leave(&dev, 1);
    return rc;
}

app_status_t app_analog_output_running(uint8_t *running)
{
    if (!running) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    uint8_t active = 0;
    if (playing) {
        rc = app_from_bsp(bsp_dac_playback_running(dac, &active));
        if (rc != APP_OK || !active) {
            app_status_t cleanup = stop();
            if (rc == APP_OK) rc = cleanup;
        }
    }
    if (rc == APP_OK) *running = active;
    app_device_leave(&dev, 1);
    return rc;
}

app_status_t app_analog_output_stop(void)
{
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    rc = stop();
    app_device_leave(&dev, 1);
    return rc;
}

app_status_t app_analog_output_de_init(void)
{
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    rc = stop();
    if (rc == APP_OK) rc = app_from_bsp(bsp_dac_de_init(dac));
    app_device_leave(&dev, rc != APP_OK);
    return rc;
}
