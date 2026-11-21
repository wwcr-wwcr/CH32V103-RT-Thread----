#include "app_ntc.h"
#include "app_internal.h"
#include <math.h>

#define NTC_AVERAGE_COUNT 16U
#define NTC_PULLUP_OHM    10000.0f
#define NTC_R25_OHM       10000.0f
static app_device_t device;

app_status_t app_ntc_init(void)
{
    app_status_t rc = app_device_enter(&device, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_adc_init(ADC1));
    if (rc == APP_OK) {
        rc = app_from_bsp(bsp_adc_channel_init(ADC1, BSP_ADC_NTC_CHANNEL, ADC_SampleTime_239Cycles5));
        if (rc != APP_OK) (void)bsp_adc_de_init(ADC1);
    }
    app_device_leave(&device, rc == APP_OK);
    return rc;
}

app_status_t app_ntc_read_raw(uint16_t *adc)
{
    uint16_t samples[NTC_AVERAGE_COUNT];
    uint32_t sum = 0;
    if (!adc) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_adc_read(ADC1, BSP_ADC_NTC_CHANNEL, samples, NTC_AVERAGE_COUNT));
    if (rc == APP_OK) {
        for (size_t i = 0; i < NTC_AVERAGE_COUNT; ++i) sum += samples[i];
        *adc = (uint16_t)((sum + NTC_AVERAGE_COUNT / 2U) / NTC_AVERAGE_COUNT);
    }
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_ntc_read(app_ntc_sample_t *sample)
{
    app_ntc_sample_t result;
    if (!sample) return APP_ERR_ARG;
    app_status_t rc = app_ntc_read_raw(&result.adc);
    if (rc != APP_OK) return rc;
    if (result.adc == 0 || result.adc >= 4095U) return APP_ERR_IO;
    result.resistance_ohm = NTC_PULLUP_OHM * result.adc / (4095U - result.adc);
    float inverse_t = 1.0f / 298.15f + logf(result.resistance_ohm / NTC_R25_OHM) / APP_NTC_BETA_K;
    if (!(inverse_t > 0.0f)) return APP_ERR_IO;
    result.temperature_c = 1.0f / inverse_t - 273.15f;
    *sample = result;
    return APP_OK;
}

app_status_t app_ntc_read_celsius(float *temperature_c)
{
    app_ntc_sample_t result;
    if (!temperature_c) return APP_ERR_ARG;
    app_status_t rc = app_ntc_read(&result);
    if (rc == APP_OK) *temperature_c = result.temperature_c;
    return rc;
}

app_status_t app_ntc_de_init(void)
{
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_adc_de_init(ADC1));
    app_device_leave(&device, rc != APP_OK);
    return rc;
}
