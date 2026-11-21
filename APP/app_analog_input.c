#include "app_analog_input.h"
#include "app_internal.h"

static app_device_t dev;
static ADC_TypeDef *adc;
static uint8_t input_channel;

app_status_t app_analog_input_init(uint8_t instance, uint8_t channel)
{
    if ((instance != 1 && instance != 2) || channel > 15) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&dev, 0);
    if (rc != APP_OK) return rc;
    adc = instance == 1 ? ADC1 : ADC2;
    rc = app_from_bsp(bsp_adc_init(adc));
    if (rc == APP_OK) {
        rc = app_from_bsp(bsp_adc_channel_init(adc, channel, ADC_SampleTime_55Cycles5));
        if (rc != APP_OK) (void)bsp_adc_de_init(adc);
    }
    input_channel = channel;
    app_device_leave(&dev, rc == APP_OK);
    return rc;
}

app_status_t app_analog_input_capture(uint32_t rate_hz, uint16_t *samples, size_t count)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_adc_read_timed(adc, input_channel, rate_hz, samples, count));
    app_device_leave(&dev, 1);
    return rc;
}

app_status_t app_analog_input_de_init(void)
{
    app_status_t rc = app_device_enter(&dev, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_adc_de_init(adc));
    app_device_leave(&dev, rc != APP_OK);
    return rc;
}
