#include "mic.h"
#include "adc.h"

int mic_read(void) { return adc_read(ADC_PA1); }

int mic_capture(uint16_t *samples, size_t count, uint32_t rate_hz)
{
    return adc_capture(ADC_PA1, samples, count, rate_hz);
}

int mic_level(uint16_t *samples, size_t count, uint32_t rate_hz)
{
    int rc = mic_capture(samples, count, rate_hz);
    if (rc < 0) return rc;
    uint16_t low = samples[0], high = samples[0];
    for (size_t i = 1; i < count; ++i) {
        if (samples[i] < low) low = samples[i];
        if (samples[i] > high) high = samples[i];
    }
    return high - low;
}
