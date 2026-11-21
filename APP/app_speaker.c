#include "app_speaker.h"
#include "app_internal.h"

#define SPEAKER_DAC 1U
#define SILENCE_CODE 2048U
static app_device_t device;

static app_status_t output_sample(int16_t sample)
{
    uint16_t code = (uint16_t)(((int32_t)sample + 32768) >> 4);
    return app_from_bsp(bsp_dac_write(SPEAKER_DAC, code));
}

app_status_t app_speaker_init(void)
{
    app_status_t result = app_device_enter(&device, 0);
    if (result != APP_OK) return result;
    result = app_from_bsp(bsp_dac_init(SPEAKER_DAC));
    if (result == APP_OK) {
        result = app_from_bsp(bsp_dac_write(SPEAKER_DAC, SILENCE_CODE));
        if (result != APP_OK) bsp_dac_de_init(SPEAKER_DAC);
    }
    app_device_leave(&device, result == APP_OK);
    return result;
}

app_status_t app_speaker_de_init(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = app_from_bsp(bsp_dac_write(SPEAKER_DAC, SILENCE_CODE));
    app_status_t release = app_from_bsp(bsp_dac_de_init(SPEAKER_DAC));
    app_device_leave(&device, release != APP_OK);
    return result == APP_OK ? release : result;
}

app_status_t app_speaker_write_sample(int16_t sample)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = output_sample(sample);
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_speaker_mute(void)
{
    return app_speaker_write_sample(0);
}

app_status_t app_speaker_play_pcm(const int16_t *samples, size_t count, uint32_t rate_hz)
{
    if (!samples || !count || rate_hz < 1000 || rate_hz > 24000) return APP_ERR_ARG;
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = app_from_bsp(bsp_tim_init_hz(TIM6, rate_hz));
    if (result == APP_OK) {
        rt_tick_t previous = rt_tick_get();
        rt_tick_t limit = rt_tick_from_millisecond(4);
        for (size_t i = 0; i < count && result == APP_OK; ++i) {
            result = output_sample(samples[i]);
            uint8_t updated = 0;
            while (result == APP_OK && !updated) {
                rt_tick_t now = rt_tick_get();
                if (now - previous >= limit) result = APP_ERR_TIMEOUT;
                else result = app_from_bsp(bsp_tim_take_update(TIM6, &updated));
                if (updated) previous = now;
            }
        }
        app_status_t mute = app_from_bsp(bsp_dac_write(SPEAKER_DAC, SILENCE_CODE));
        app_status_t release = app_from_bsp(bsp_tim_de_init(TIM6));
        if (result == APP_OK) result = mute == APP_OK ? release : mute;
    }
    app_device_leave(&device, 1);
    return result;
}
