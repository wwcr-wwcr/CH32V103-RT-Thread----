#include "app_encoder.h"
#include "app_internal.h"

#define ENCODER_FILTER 6U
static app_device_t encoder;
static int64_t position;

app_status_t app_encoder_init(void)
{
    app_status_t result = app_device_enter(&encoder, 0);
    if (result != APP_OK) return result;

    bsp_status_t status = bsp_tim_encoder_init(TIM3, 0, ENCODER_FILTER);
    if (status == BSP_OK) position = 0;
    app_device_leave(&encoder, status == BSP_OK);
    return app_from_bsp(status);
}

app_status_t app_encoder_read(app_encoder_sample_t *sample)
{
    if (!sample) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&encoder, 1);
    if (result != APP_OK) return result;

    int16_t delta;
    bsp_status_t status = bsp_tim_encoder_delta(TIM3, &delta);
    if (status == BSP_OK) {
        position += delta;
        sample->delta = delta;
        sample->position = position;
    }
    app_device_leave(&encoder, 1);
    return app_from_bsp(status);
}

app_status_t app_encoder_zero(void)
{
    app_status_t result = app_device_enter(&encoder, 1);
    if (result != APP_OK) return result;

    int16_t discarded;
    bsp_status_t status = bsp_tim_encoder_delta(TIM3, &discarded);
    if (status == BSP_OK) position = 0;
    app_device_leave(&encoder, 1);
    return app_from_bsp(status);
}

app_status_t app_encoder_de_init(void)
{
    app_status_t result = app_device_enter(&encoder, 1);
    if (result != APP_OK) return result;
    bsp_status_t status = bsp_tim_encoder_de_init(TIM3);
    app_device_leave(&encoder, status != BSP_OK);
    return app_from_bsp(status);
}
