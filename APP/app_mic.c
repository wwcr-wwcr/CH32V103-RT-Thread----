#include "app_mic.h"
#include "app_internal.h"
#include <math.h>

#define MIC_OPA_OUTPUT_CHANNEL ADC_Channel_1  /* OPA3_OUT0，PA1 */

static app_device_t device;
static uint8_t has_frame;
static app_mic_level_t last;
static uint16_t measurement_frame[APP_MIC_MEASURE_SAMPLES];
static uint8_t calibrated;
static float spl_offset;
#define MIN_MEASURABLE_RMS 4.0f
static uint8_t owns_adc, owns_opa;

static app_status_t release_mic(void)
{
    app_status_t rc = APP_OK;
    if (owns_adc) {
        rc = app_from_bsp(bsp_adc_de_init(ADC2));
        if (rc == APP_OK) owns_adc = 0;
    }
    if (owns_opa) {
        app_status_t opa_rc = app_from_bsp(bsp_opa_de_init(OPA3));
        if (opa_rc == APP_OK) owns_opa = 0;
        if (rc == APP_OK) rc = opa_rc;
    }
    return rc;
}

app_status_t app_mic_init(void)
{
    app_status_t rc = app_device_enter(&device, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_opa_init(OPA3, CHP1, CHN1, OUT_IO_OUT0));
    if (rc == APP_OK) {
        owns_opa = 1;
        rc = app_from_bsp(bsp_adc_init(ADC2));
    }
    if (rc == APP_OK) {
        owns_adc = 1;
        rc = app_from_bsp(bsp_adc_channel_init(ADC2, MIC_OPA_OUTPUT_CHANNEL, ADC_SampleTime_55Cycles5));
    }
    has_frame = calibrated = 0;
    if (rc != APP_OK) (void)release_mic();
    app_device_leave(&device, owns_adc || owns_opa);
    return rc;
}

static void analyze_level(const uint16_t *samples, size_t count)
{
    uint64_t sum = 0, squares = 0;
    app_mic_level_t level = {0};
    level.min_adc = 4095U;
    for (size_t i = 0; i < count; ++i) {
        uint16_t v = samples[i];
        if (v < level.min_adc) level.min_adc = v;
        if (v > level.max_adc) level.max_adc = v;
        if (v == 0U || v == 4095U) ++level.clipped_samples;
        sum += v;
        squares += (uint32_t)v * v;
    }
    level.peak_to_peak = level.max_adc - level.min_adc;
    level.mean_adc = (float)sum / count;
    /* 先用整数消去直流，避免小交流信号在浮点消减中丢失。 */
    level.rms_adc = sqrtf((float)(squares * count - sum * sum)) / count;
    level.dbfs = level.rms_adc > 0 ? 20.0f * log10f(level.rms_adc / 2047.5f) : -INFINITY;
    last = level;
    has_frame = 1;
}

app_status_t app_mic_capture(uint16_t *samples, size_t count)
{
    if (!samples || count < 2U || count > APP_MIC_MAX_SAMPLES) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    has_frame = 0;
    if (!owns_opa || !owns_adc) rc = APP_ERR_STATE;
    else rc = app_from_bsp(bsp_adc_read(ADC2, MIC_OPA_OUTPUT_CHANNEL, samples, count));
    if (rc == APP_OK) analyze_level(samples, count);
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_mic_update(void)
{
    return app_mic_capture(measurement_frame, APP_MIC_FRAME_SAMPLES);
}

app_status_t app_mic_get_level(app_mic_level_t *level)
{
    if (!level) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    if (!has_frame) rc = APP_ERR_STATE;
    else *level = last;
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_mic_get_mean(float *value)
{
    app_mic_level_t level;
    if (!value) return APP_ERR_ARG;
    app_status_t rc = app_mic_get_level(&level);
    if (rc == APP_OK) *value = level.mean_adc;
    return rc;
}

app_status_t app_mic_get_peak_to_peak(uint16_t *value)
{
    app_mic_level_t level;
    if (!value) return APP_ERR_ARG;
    app_status_t rc = app_mic_get_level(&level);
    if (rc == APP_OK) *value = level.peak_to_peak;
    return rc;
}

app_status_t app_mic_get_rms(float *value)
{
    app_mic_level_t level;
    if (!value) return APP_ERR_ARG;
    app_status_t rc = app_mic_get_level(&level);
    if (rc == APP_OK) *value = level.rms_adc;
    return rc;
}

app_status_t app_mic_get_dbfs(float *value)
{
    app_mic_level_t level;
    if (!value) return APP_ERR_ARG;
    app_status_t rc = app_mic_get_level(&level);
    if (rc == APP_OK) *value = level.dbfs;
    return rc;
}

/* 调用方已锁定设备；采集和读取其结果始终在同一锁内。 */
static app_status_t measure_frame(void)
{
    if (!owns_adc || !owns_opa || !app_can_sleep()) return APP_ERR_STATE;
    has_frame = 0;
    app_status_t rc = app_from_bsp(bsp_adc_read_timed(ADC2, MIC_OPA_OUTPUT_CHANNEL,
                                   APP_MIC_MEASURE_RATE_HZ, measurement_frame, APP_MIC_MEASURE_SAMPLES));
    if (rc == APP_OK) analyze_level(measurement_frame, APP_MIC_MEASURE_SAMPLES);
    return rc;
}

/* 迟滞带过滤直流附近的小毛刺。用上升过零点插值计时，检查每个完整周期，
 * 再以首末过零点的总时长估计频率，减小非整数采样周期的量化误差。 */
static app_status_t tone_frequency(float *hz)
{
    if (last.rms_adc < MIN_MEASURABLE_RMS) return APP_ERR_NOT_FOUND;
    int mid = (int)(last.mean_adc + 0.5f);
    int band = (int)(last.rms_adc * 0.25f);
    int armed = measurement_frame[0] <= mid - band;
    float crossing = -1, first = 0, previous = 0;
    float shortest = APP_MIC_MEASURE_SAMPLES, longest = 0;
    unsigned edges = 0;
    for (unsigned i = 1; i < APP_MIC_MEASURE_SAMPLES; ++i) {
        int a = measurement_frame[i - 1], b = measurement_frame[i];
        if (b <= mid - band) {
            armed = 1;
            crossing = -1;
        }
        if (!armed) continue;
        if (a <= mid && b > mid)
            crossing = (float)(i - 1U) + (float)(mid - a) / (float)(b - a);
        if (b < mid + band || crossing < 0) continue;
        if (edges) {
            float period = crossing - previous;
            if (period < shortest) shortest = period;
            if (period > longest) longest = period;
        } else first = crossing;
        previous = crossing;
        ++edges;
        armed = 0;
    }
    if (edges < 3) return APP_ERR_NOT_FOUND;
    float period = (previous - first) / (float)(edges - 1U);
    float tolerance = period * 0.12f + 0.4f;
    if (period - shortest > tolerance || longest - period > tolerance) return APP_ERR_NOT_FOUND;
    float frequency = APP_MIC_MEASURE_RATE_HZ / period;
    /* 量程两端允许 0.1% 的过零插值误差。 */
    if (frequency < 99.9f || frequency > 10010.0f) return APP_ERR_NOT_FOUND;
    *hz = frequency;
    return APP_OK;
}

app_status_t app_mic_read_frequency_hz(float *frequency_hz)
{
    if (!frequency_hz) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = measure_frame();
    if (rc == APP_OK) rc = tone_frequency(frequency_hz);
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_mic_read_dbfs(float *dbfs)
{
    if (!dbfs) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = measure_frame();
    if (rc == APP_OK) *dbfs = last.dbfs;
    app_device_leave(&device, 1);
    return rc;
}

static app_status_t measure_spl_frame(void)
{
    app_status_t rc = measure_frame();
    if (rc == APP_OK && (last.min_adc < APP_MIC_SPL_MIN_ADC || last.max_adc > APP_MIC_SPL_MAX_ADC ||
                         last.rms_adc < MIN_MEASURABLE_RMS)) rc = APP_ERR_IO;
    return rc;
}

app_status_t app_mic_calibrate_spl(float reference_db_spl)
{
    if (!isfinite(reference_db_spl)) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = measure_spl_frame();
    if (rc == APP_OK) {
        spl_offset = reference_db_spl - last.dbfs;
        calibrated = 1;
    }
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_mic_read_spl(float *db_spl)
{
    if (!db_spl) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = calibrated ? measure_spl_frame() : APP_ERR_STATE;
    if (rc == APP_OK) *db_spl = spl_offset + last.dbfs;
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_mic_de_init(void)
{
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    has_frame = calibrated = 0;
    rc = release_mic();
    app_device_leave(&device, owns_adc || owns_opa);
    return rc;
}
