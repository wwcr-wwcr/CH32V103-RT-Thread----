#include "app_hal_wave_private.h"
#include "app_analog_input.h"
#include <math.h>
#include <string.h>

#if APP_HAL_WAVE_FRAME_SAMPLES < 16 || APP_HAL_WAVE_FRAME_SAMPLES > 4096 || APP_HAL_WAVE_STREAM_ROWS_PER_UPDATE < 1
#error Invalid ADC waveform frame configuration
#endif
#if APP_HAL_WAVE_MIN_PP_CODES < 1 || APP_HAL_WAVE_MIN_PP_CODES > 4095 || APP_HAL_WAVE_HYSTERESIS_PCT < 1 || APP_HAL_WAVE_HYSTERESIS_PCT >= 50
#error Invalid minimum waveform swing or hysteresis
#endif
#if APP_HAL_WAVE_SQUARE_RAIL_PCT < 1 || APP_HAL_WAVE_SQUARE_RAIL_PCT > 100 || APP_HAL_WAVE_SQUARE_PLATEAU_PCT < 1 || APP_HAL_WAVE_SQUARE_PLATEAU_PCT > 100
#error Invalid square wave thresholds
#endif
static uint16_t frame[APP_HAL_WAVE_FRAME_SAMPLES], reference_mv;
static uint32_t rate;
uint8_t hal_wave_loopback_call;
static struct {
    uint32_t interval, last;
    unsigned row;
    uint8_t enabled, raw, pending, first, writing;
    app_hal_wave_write_t write;
    void *context;
    app_hal_wave_report_t report;
} stream;

app_status_t app_hal_wave_input_init(uint8_t adc, uint8_t channel, uint32_t rate_hz, uint16_t vref_mv)
{
    if (!rate_hz) rate_hz = APP_HAL_WAVE_RATE_HZ;
    if (rate_hz < 2000U || rate_hz > 48000U) return APP_ERR_ARG;
    app_status_t rc = hal_open(HAL_WAVE_INPUT, 0);
    if (rc != APP_OK) return rc;
    rc = app_analog_input_init(adc, channel);
    if (rc != APP_OK) { (void)hal_close(HAL_WAVE_INPUT); return rc; }
    reference_mv = vref_mv ? vref_mv : APP_HAL_WAVE_VREF_MV;
    rate = rate_hz;
    memset(&stream, 0, sizeof stream);
    return APP_OK;
}

app_status_t app_hal_wave_input_de_init(void)
{
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (stream.writing) return APP_ERR_BUSY;
    app_status_t rc = app_analog_input_de_init();
    if (rc != APP_OK) return rc;
    stream.enabled = 0;
    return hal_close(HAL_WAVE_INPUT);
}

/* Use full cycles between rising midpoint crossings. Schmitt bands prevent
 * noise close to the midpoint from creating extra cycles. */
static void periodic(const uint16_t *s, size_t n, uint32_t hz, app_hal_wave_report_t *r)
{
    float amplitude = 0.5f * (r->max_code - r->min_code);
    float mid = r->min_code + amplitude;
    float band = amplitude * (APP_HAL_WAVE_HYSTERESIS_PCT / 50.0f);
    int high = s[0] >= mid;
    float candidate = -1, first = -1, last = -1, falling = -1, highs = 0;
    float shortest = n, longest = 0;
    unsigned cycles = 0, rails = 0;
    for (size_t i = 1; i < n; ++i) {
        int a = s[i - 1], b = s[i];
        if (!high && a <= mid && b > mid)
            candidate = (float)(i - 1U) + (mid - a) / (b - a);
        if (high && a >= mid && b < mid)
            candidate = (float)(i - 1U) + (mid - a) / (b - a);
        if (candidate < 0) continue;
        if (!high && b >= mid + band) {
            if (last >= 0 && falling > last) {
                float length = candidate - last;
                if (length < shortest) shortest = length;
                if (length > longest) longest = length;
                highs += falling - last;
                ++cycles;
            } else first = candidate;
            last = candidate;
            high = 1;
            candidate = -1;
        } else if (high && b <= mid - band) {
            falling = candidate;
            high = 0;
            candidate = -1;
        }
    }
    if (cycles < 2) return;
    float period = (last - first) / cycles;
    float allowed = period * (APP_HAL_WAVE_PERIOD_TOL_PCT / 100.0f) + 0.4f;
    if (period < 4 || period - shortest > allowed || longest - period > allowed) return;
    r->periodic = 1;
    r->frequency_hz = hz / period;
    float high_samples = highs / cycles;
    r->high_us = 1000000.0f * high_samples / hz;
    r->low_us = 1000000.0f * (period - high_samples) / hz;
    r->duty_percent = 100.0f * high_samples / period;
    if (period < 8) return; /* Too few samples per cycle to distinguish shapes. */
    float sine_error = 0, tri_error = 0;
    unsigned begin = (unsigned)first + 1U, end = (unsigned)last;
    float step = 1.0f / period;
    float phase = (begin - first) * step;
    for (unsigned i = begin; i <= end; ++i) {
        float value = (s[i] - mid) / amplitude;
        if (fabsf(value) >= APP_HAL_WAVE_SQUARE_RAIL_PCT / 100.0f) ++rails;
        float a = value - wave_sine(phase), b = value - wave_triangle(phase);
        sine_error += a * a;
        tri_error += b * b;
        phase += step;
        if (phase >= 1.0f) phase -= 1.0f;
    }
    unsigned points = end - begin + 1U;
    float limit = APP_HAL_WAVE_SHAPE_ERROR_PCT / 100.0f;
    if (rails * 100U >= points * APP_HAL_WAVE_SQUARE_PLATEAU_PCT) r->kind = APP_HAL_WAVE_SQUARE;
    else if (sine_error < tri_error && sine_error <= points * limit * limit) r->kind = APP_HAL_WAVE_SINE;
    else if (tri_error <= points * limit * limit) r->kind = APP_HAL_WAVE_TRIANGLE;
}

app_status_t app_hal_wave_analyze(const uint16_t *s, size_t n, uint32_t hz, uint16_t ref, app_hal_wave_report_t *out)
{
    if (!s || !out || n < 2 || n > 4096 || !hz || !ref) return APP_ERR_ARG;
    uint64_t sum = 0, squares = 0;
    app_hal_wave_report_t r = {0};
    r.min_code = 4095;
    for (size_t i = 0; i < n; ++i) {
        unsigned v = s[i];
        if (v > 4095) return APP_ERR_ARG;
        if (v < r.min_code) r.min_code = v;
        if (v > r.max_code) r.max_code = v;
        sum += v;
        squares += v * v;
    }
    float scale = ref / 4095.0f;
    unsigned span = r.max_code - r.min_code;
    r.mean_mv = (float)sum * scale / n;
    r.ac_rms_mv = sqrtf((float)(squares * n - sum * sum)) * scale / n;
    r.peak_to_peak_mv = span * scale;
    r.clipped = !r.min_code || r.max_code == 4095;
    if (span >= APP_HAL_WAVE_MIN_PP_CODES) periodic(s, n, hz, &r);
    *out = r;
    return APP_OK;
}

static app_status_t capture_report(app_hal_wave_report_t *out)
{
    app_status_t rc = app_analog_input_capture(rate, frame, APP_HAL_WAVE_FRAME_SAMPLES);
    if (rc == APP_OK) rc = app_hal_wave_analyze(frame, APP_HAL_WAVE_FRAME_SAMPLES, rate, reference_mv, out);
    return rc;
}

app_status_t app_hal_wave_measure(app_hal_wave_report_t *out)
{
    if (!out) return APP_ERR_ARG;
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (stream.enabled) return APP_ERR_BUSY;
    return capture_report(out);
}

app_status_t app_hal_pulse_measure(app_hal_wave_report_t *out)
{
    if (!out) return APP_ERR_ARG;
    app_hal_wave_report_t r;
    app_status_t rc = app_hal_wave_measure(&r);
    if (rc == APP_OK && !r.periodic) rc = APP_ERR_NOT_FOUND;
    if (rc == APP_OK) *out = r;
    return rc;
}

app_status_t app_hal_wave_capture_triggered(uint16_t level_mv, uint8_t rising, uint32_t timeout_ms,
                                           uint16_t *out, size_t count)
{
    if (!out || !count || count > APP_HAL_WAVE_FRAME_SAMPLES / 2 || !timeout_ms || rising > 1)
        return APP_ERR_ARG;
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (stream.enabled) return APP_ERR_BUSY;
    if (wave_taken()) return APP_ERR_BUSY;
    if (!level_mv || level_mv >= reference_mv) return APP_ERR_ARG;
    uint32_t threshold = ((uint32_t)level_mv * 4095U + reference_mv / 2U) / reference_mv;
    uint32_t start = hal_now_ms();
    for (;;) {
        uint32_t elapsed = hal_now_ms() - start;
        if (elapsed >= timeout_ms) return APP_ERR_TIMEOUT;
        uint64_t budget = (uint64_t)(timeout_ms - elapsed) * rate / 1000U;
        size_t length = budget < APP_HAL_WAVE_FRAME_SAMPLES ? (size_t)budget : APP_HAL_WAVE_FRAME_SAMPLES;
        if (length <= count) return APP_ERR_TIMEOUT;
        app_status_t rc = app_analog_input_capture(rate, frame, length);
        if (rc != APP_OK) return rc;
        if (hal_now_ms() - start > timeout_ms) return APP_ERR_TIMEOUT;
        for (size_t i = 1; i + count <= length; ++i) {
            int edge = rising ? (frame[i-1] < threshold && frame[i] >= threshold) :
                                (frame[i-1] > threshold && frame[i] <= threshold);
            if (edge) { memcpy(out, frame + i, count * sizeof(*out)); return APP_OK; }
        }
    }
}

app_status_t hal_wave_send(const char *line, size_t length, app_hal_wave_write_t write, void *context)
{
    if (write) return write(line, length, context);
    rt_device_t dev = rt_console_get_device();
    if (!dev) return APP_ERR_STATE;
    return rt_device_write(dev, 0, line, length) == length ? APP_OK : APP_ERR_IO;
}

app_status_t app_hal_wave_stream_start(uint32_t interval_ms, uint8_t raw, app_hal_wave_write_t write, void *context)
{
    if (raw > 1) return APP_ERR_ARG;
    if (wave_taken()) return APP_ERR_BUSY;
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (stream.enabled) return APP_ERR_BUSY;
    stream.interval = interval_ms ? interval_ms : APP_HAL_WAVE_STREAM_INTERVAL_MS;
    stream.raw = raw;
    stream.write = write;
    stream.context = context;
    stream.pending = 0;
    stream.enabled = stream.first = 1;
    hal_schedule(HAL_WAVE_INPUT, app_hal_wave_input_poll);
    return APP_OK;
}

app_status_t app_hal_wave_stream_stop(void)
{
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (stream.writing) return APP_ERR_BUSY;
    stream.enabled = stream.pending = 0;
    hal_schedule(HAL_WAVE_INPUT, NULL);
    return APP_OK;
}

app_status_t app_hal_wave_input_poll(void)
{
    if (!hal_active(HAL_WAVE_INPUT)) return APP_ERR_STATE;
    if (!stream.enabled) return APP_OK;
    if (stream.writing) return APP_ERR_BUSY;
    if (!stream.pending) {
        if (!stream.first && hal_now_ms() - stream.last < stream.interval) return APP_OK;
        app_status_t rc = capture_report(&stream.report);
        if (rc != APP_OK) return rc;
        stream.first = 0;
        stream.pending = 1;
        stream.row = 0;
    }
    for (unsigned i = 0; i < APP_HAL_WAVE_STREAM_ROWS_PER_UPDATE; ++i) {
        char line[100];
        int length;
        if (!stream.row) {
            static const char *const names[] = {"unknown", "sine", "triangle", "square"};
            const app_hal_wave_report_t *r = &stream.report;
            length = rt_snprintf(line, sizeof line, "wave,%s,%u,%u,%u,%u,%u,%u\n", names[r->kind],
                            (unsigned)(r->frequency_hz + 0.5f), (unsigned)(r->peak_to_peak_mv + 0.5f),
                            (unsigned)(r->mean_mv + 0.5f), (unsigned)(r->ac_rms_mv + 0.5f),
                            (unsigned)(r->duty_percent * 10.0f + 0.5f), (unsigned)r->clipped);
        } else length = rt_snprintf(line, sizeof line, "sample,%u,%u\n",
                                    stream.row - 1U, (unsigned)frame[stream.row - 1U]);
        stream.writing = 1;
        app_status_t rc = hal_wave_send(line, (size_t)length, stream.write, stream.context);
        stream.writing = 0;
        if (rc != APP_OK) {
            if (rc != APP_ERR_BUSY) (void)app_hal_wave_stream_stop();
            return rc;
        }
        if (++stream.row > APP_HAL_WAVE_FRAME_SAMPLES || !stream.raw) {
            stream.pending = 0;
            stream.last = hal_now_ms();
            break;
        }
    }
    return APP_OK;
}
