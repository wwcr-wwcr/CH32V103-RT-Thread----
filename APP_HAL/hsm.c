/* RT-Thread MSH serial entry for the board's APP_HAL functions. */
#include "app_hal.h"
#include <rtthread.h>
#include <finsh.h>
#include <string.h>
#include <math.h>
#include <limits.h>

typedef union { uint32_t positive; int32_t integer; float real; } number_t;
typedef struct { const char *name, *types, *usage; } command_t;

enum command_id {
    CMD_METER_INIT,
    CMD_METER_DEINIT,
    CMD_TIMER_INIT,
    CMD_TIMER_DEINIT,
    CMD_CLOCK_INIT,
    CMD_CLOCK_DEINIT,
    CMD_COUNTER_INIT,
    CMD_COUNTER_DEINIT,
    CMD_PARAM_INIT,
    CMD_PARAM_DEINIT,
    CMD_ALARM_INIT,
    CMD_ALARM_DEINIT,
    CMD_FAN_INIT,
    CMD_FAN_DEINIT,
    CMD_PROXIMITY_INIT,
    CMD_PROXIMITY_DEINIT,
    CMD_NOISE_INIT,
    CMD_NOISE_DEINIT,
    CMD_RPM_INIT,
    CMD_RPM_DEINIT,
    CMD_TILT_INIT,
    CMD_TILT_DEINIT,
    CMD_METER_PAGE,
    CMD_METER_CAL,
    CMD_METER_HOLD,
    CMD_METER_VIEW,
    CMD_METER_RESET,
    CMD_TIMER_DOWN,
    CMD_TIMER_UP,
    CMD_TIMER_PAUSE,
    CMD_TIMER_RESUME,
    CMD_TIMER_STOP,
    CMD_TIMER_ACK,
    CMD_CLOCK_SHOW,
    CMD_CLOCK_EDIT,
    CMD_CLOCK_FINISH,
    CMD_COUNTER_START,
    CMD_COUNTER_STEP,
    CMD_COUNTER_TARGET,
    CMD_PARAM_FINISH,
    CMD_ALARM_DEFAULT,
    CMD_ALARM_SET,
    CMD_ALARM_OFF,
    CMD_ALARM_ACK,
    CMD_MIC_CAL,
    CMD_FAN_SET,
    CMD_PROXIMITY_SET,
    CMD_PROXIMITY_SHOW,
    CMD_PROXIMITY_RESET,
    CMD_NOISE_SET,
    CMD_NOISE_RESET,
    CMD_RPM_SHOW,
    CMD_TILT_SHOW,
    CMD_TILT_SET,
    CMD_ADC_INIT,
    CMD_ADC_DEINIT,
    CMD_ADC_STREAM,
    CMD_ADC_STOP,
    CMD_DAC_INIT,
    CMD_DAC_DEINIT,
    CMD_DAC_STOP,
    CMD_LOOP_INIT,
    CMD_LOOP_DEINIT,
    CMD_CORE_INIT,
    CMD_CORE_DEINIT,
    CMD_COUNTER_CONFIG,
    CMD_PARAM_EDIT,
    CMD_TIMER_GET,
    CMD_TIMER_LAP,
    CMD_METER_GET,
    CMD_COUNTER_GET,
    CMD_FAN_GET,
    CMD_PROXIMITY_GET,
    CMD_NOISE_GET,
    CMD_RPM_GET,
    CMD_TILT_GET,
    CMD_ALARM_GET,
    CMD_DAC_GET,
    CMD_PARAM_GET,
    CMD_ADC_MEASURE,
    CMD_ADC_PULSE,
    CMD_ADC_TRIGGER,
    CMD_ADC_ANALYZE,
    CMD_DAC_START,
    CMD_DAC_BURST,
    CMD_LOOP_TEST,
};
static const command_t commands[] = {
    [CMD_METER_INIT] = {"meter.init", ""},
    [CMD_METER_DEINIT] = {"meter.deinit", ""},
    [CMD_TIMER_INIT] = {"timer.init", ""},
    [CMD_TIMER_DEINIT] = {"timer.deinit", ""},
    [CMD_CLOCK_INIT] = {"clock.init", ""},
    [CMD_CLOCK_DEINIT] = {"clock.deinit", ""},
    [CMD_COUNTER_INIT] = {"counter.init", ""},
    [CMD_COUNTER_DEINIT] = {"counter.deinit", ""},
    [CMD_PARAM_INIT] = {"param.init", ""},
    [CMD_PARAM_DEINIT] = {"param.deinit", ""},
    [CMD_ALARM_INIT] = {"alarm.init", ""},
    [CMD_ALARM_DEINIT] = {"alarm.deinit", ""},
    [CMD_FAN_INIT] = {"fan.init", ""},
    [CMD_FAN_DEINIT] = {"fan.deinit", ""},
    [CMD_PROXIMITY_INIT] = {"proximity.init", ""},
    [CMD_PROXIMITY_DEINIT] = {"proximity.deinit", ""},
    [CMD_NOISE_INIT] = {"noise.init", ""},
    [CMD_NOISE_DEINIT] = {"noise.deinit", ""},
    [CMD_RPM_INIT] = {"rpm.init", "u", "counts_per_rev"},
    [CMD_RPM_DEINIT] = {"rpm.deinit", ""},
    [CMD_TILT_INIT] = {"tilt.init", ""},
    [CMD_TILT_DEINIT] = {"tilt.deinit", ""},
    [CMD_METER_PAGE] = {"meter.page", "b", "page"},
    [CMD_METER_CAL] = {"meter.cal", "f", "ref_dbspl"},
    [CMD_METER_HOLD] = {"meter.hold", "b", "hold"},
    [CMD_METER_VIEW] = {"meter.view", "b", "view"},
    [CMD_METER_RESET] = {"meter.reset", ""},
    [CMD_TIMER_DOWN] = {"timer.down", "ub", "ms alarm"},
    [CMD_TIMER_UP] = {"timer.up", ""},
    [CMD_TIMER_PAUSE] = {"timer.pause", ""},
    [CMD_TIMER_RESUME] = {"timer.resume", ""},
    [CMD_TIMER_STOP] = {"timer.stop", ""},
    [CMD_TIMER_ACK] = {"timer.ack", ""},
    [CMD_CLOCK_SHOW] = {"clock.show", ""},
    [CMD_CLOCK_EDIT] = {"clock.edit", ""},
    [CMD_CLOCK_FINISH] = {"clock.finish", "b", "save"},
    [CMD_COUNTER_START] = {"counter.start", ""},
    [CMD_COUNTER_STEP] = {"counter.step", "i", "direction"},
    [CMD_COUNTER_TARGET] = {"counter.target", "i", "target"},
    [CMD_PARAM_FINISH] = {"param.finish", "b", "save"},
    [CMD_ALARM_DEFAULT] = {"alarm.default", "b", "source"},
    [CMD_ALARM_SET] = {"alarm.set", "bffu", "source trip reset confirm_ms"},
    [CMD_ALARM_OFF] = {"alarm.off", "b", "source"},
    [CMD_ALARM_ACK] = {"alarm.ack", "b", "source"},
    [CMD_MIC_CAL] = {"mic.cal", "f", "ref_dbspl"},
    [CMD_FAN_SET] = {"fan.set", "fffb", "stop_c start_c full_c min_pct"},
    [CMD_PROXIMITY_SET] = {"proximity.set", "ww", "near_mm far_mm"},
    [CMD_PROXIMITY_SHOW] = {"proximity.show", "b", "count_page"},
    [CMD_PROXIMITY_RESET] = {"proximity.reset", ""},
    [CMD_NOISE_SET] = {"noise.set", "f", "limit_dbspl"},
    [CMD_NOISE_RESET] = {"noise.reset", ""},
    [CMD_RPM_SHOW] = {"rpm.show", ""},
    [CMD_TILT_SHOW] = {"tilt.show", ""},
    [CMD_TILT_SET] = {"tilt.set", "ffu", "trip_deg reset_deg confirm_ms"},
    [CMD_ADC_INIT] = {"adc.init", "bbuw", "adc channel sample_hz vref_mv"},
    [CMD_ADC_DEINIT] = {"adc.deinit", ""},
    [CMD_ADC_STREAM] = {"adc.stream", "ub", "interval_ms raw"},
    [CMD_ADC_STOP] = {"adc.stop", ""},
    [CMD_DAC_INIT] = {"dac.init", "bw", "channel vref_mv"},
    [CMD_DAC_DEINIT] = {"dac.deinit", ""},
    [CMD_DAC_STOP] = {"dac.stop", ""},
    [CMD_LOOP_INIT] = {"loop.init", "bbbuw", "dac_channel adc adc_channel sample_hz vref_mv"},
    [CMD_LOOP_DEINIT] = {"loop.deinit", ""},
    [CMD_CORE_INIT] = {"core.init", ""},
    [CMD_CORE_DEINIT] = {"core.deinit", ""},
    [CMD_COUNTER_CONFIG] = {"counter.config", "iiiiibb", "min max initial step target wrap notify"},
    [CMD_PARAM_EDIT] = {"param.edit", "iiiib", "initial min max step decimals"},
    [CMD_TIMER_GET] = {"timer.get", ""},
    [CMD_TIMER_LAP] = {"timer.lap", ""},
    [CMD_METER_GET] = {"meter.get", "b", "page"},
    [CMD_COUNTER_GET] = {"counter.get", ""},
    [CMD_FAN_GET] = {"fan.get", ""},
    [CMD_PROXIMITY_GET] = {"proximity.get", ""},
    [CMD_NOISE_GET] = {"noise.get", ""},
    [CMD_RPM_GET] = {"rpm.get", ""},
    [CMD_TILT_GET] = {"tilt.get", ""},
    [CMD_ALARM_GET] = {"alarm.get", "b", "source"},
    [CMD_DAC_GET] = {"dac.get", ""},
    [CMD_PARAM_GET] = {"param.get", ""},
    [CMD_ADC_MEASURE] = {"adc.measure", ""},
    [CMD_ADC_PULSE] = {"adc.pulse", ""},
    [CMD_ADC_TRIGGER] = {"adc.trigger", "wbuu", "threshold_mv rising timeout_ms points"},
    [CMD_ADC_ANALYZE] = {"adc.analyze", "uw", "sample_hz vref_mv"},
    [CMD_DAC_START] = {"dac.start", "buww", "shape hz mvpp offset_mv"},
    [CMD_DAC_BURST] = {"dac.burst", "buwwu", "shape hz mvpp offset_mv cycles"},
    [CMD_LOOP_TEST] = {"loop.test", "buww", "shape hz mvpp offset_mv"},
};

/* Parse a complete decimal token and check the destination width before narrowing. */
static int parse(const char *p, char type, number_t *out)
{
    int neg = *p == '-';
    if (neg || *p == '+') ++p;
    if ((neg && type != 'i' && type != 'f') || *p < '0' || *p > '9') return 0;
    uint32_t limit = type == 'b' ? UINT8_MAX : type == 'w' ? UINT16_MAX :
                     type == 'i' ? (uint32_t)INT32_MAX + neg : UINT32_MAX;
    uint32_t value = 0;
    do {
        unsigned digit = *p++ - '0';
        if (value > (limit - digit) / 10U) return 0;
        value = value * 10U + digit;
    } while (*p >= '0' && *p <= '9');
    if (type == 'f') {
        float v = (float)value;
        if (*p == '.') {
            float place = 0.1f;
            ++p;
            if (*p < '0' || *p > '9') return 0;
            while (*p >= '0' && *p <= '9') { v += (*p++ - '0') * place; place *= 0.1f; }
        }
        out->real = neg ? -v : v;
    } else if (type == 'i')
        out->integer = neg ? (value ? -(int32_t)(value - 1U) - 1 : 0) : (int32_t)value;
    else out->positive = value;
    return !*p;
}

static void integer(const char *key, int32_t n) { rt_kprintf("%s=%d ", key, (int)n); }
static void unsigned_field(const char *key, uint32_t n) { rt_kprintf("%s=%u ", key, (unsigned)n); }
static void wide(const char *key, uint64_t n)
{
    char buf[21];
    char *p = buf + sizeof buf;
    *--p = 0;
    do { *--p = '0' + n % 10; n /= 10; } while (n);
    rt_kprintf("%s%s ", key, p);
}
static void signed_wide(const char *key, int64_t n)
{
    rt_kprintf("%s=", key);
    wide(n < 0 ? "-" : "", n < 0 ? 0ULL - (uint64_t)n : (uint64_t)n);
}
static void decimal(const char *key, float n)
{
    if (!isfinite(n)) { rt_kprintf("%s=invalid ", key); return; }
    float mag = fabsf(n);
    if (mag >= 4294967296.0f) { rt_kprintf("%s=overflow ", key); return; }
    uint32_t whole = (uint32_t)mag;
    unsigned part = (unsigned)((mag - whole) * 1000.0f + 0.5f);
    if (part == 1000) { ++whole; part = 0; }
    rt_kprintf("%s=%s%u.%03u ", key, n < 0 ? "-" : "", (unsigned)whole, part);
}
static void print_wave(const app_hal_wave_report_t *v)
{
    unsigned_field("kind", v->kind);
    unsigned_field("periodic", v->periodic);
    unsigned_field("clipped", v->clipped);
    unsigned_field("min_code", v->min_code);
    unsigned_field("max_code", v->max_code);
    decimal("frequency_hz", v->frequency_hz);
    decimal("peak_to_peak_mv", v->peak_to_peak_mv);
    decimal("mean_mv", v->mean_mv);
    decimal("ac_rms_mv", v->ac_rms_mv);
    decimal("duty_percent", v->duty_percent);
    decimal("high_us", v->high_us);
    decimal("low_us", v->low_us);
}
static uint16_t capture[APP_HAL_WAVE_FRAME_SAMPLES / 2];
static unsigned captured;

#define u(index) args[index].positive
#define i(index) args[index].integer
#define f(index) args[index].real
static app_status_t dispatch(unsigned command, const number_t *args)
{
    app_status_t rc;
    switch (command) {
    case CMD_METER_INIT: {
        return app_hal_meter_init();
    }
    case CMD_METER_DEINIT: {
        return app_hal_meter_de_init();
    }
    case CMD_TIMER_INIT: {
        return app_hal_timer_init();
    }
    case CMD_TIMER_DEINIT: {
        return app_hal_timer_de_init();
    }
    case CMD_CLOCK_INIT: {
        return app_hal_clock_init();
    }
    case CMD_CLOCK_DEINIT: {
        return app_hal_clock_de_init();
    }
    case CMD_COUNTER_INIT: {
        return app_hal_counter_init();
    }
    case CMD_COUNTER_DEINIT: {
        return app_hal_counter_de_init();
    }
    case CMD_PARAM_INIT: {
        return app_hal_param_init();
    }
    case CMD_PARAM_DEINIT: {
        return app_hal_param_de_init();
    }
    case CMD_ALARM_INIT: {
        return app_hal_alarm_init();
    }
    case CMD_ALARM_DEINIT: {
        return app_hal_alarm_de_init();
    }
    case CMD_FAN_INIT: {
        return app_hal_fan_init();
    }
    case CMD_FAN_DEINIT: {
        return app_hal_fan_de_init();
    }
    case CMD_PROXIMITY_INIT: {
        return app_hal_proximity_init();
    }
    case CMD_PROXIMITY_DEINIT: {
        return app_hal_proximity_de_init();
    }
    case CMD_NOISE_INIT: {
        return app_hal_noise_init();
    }
    case CMD_NOISE_DEINIT: {
        return app_hal_noise_de_init();
    }
    case CMD_RPM_INIT: {
        return app_hal_rpm_init(u(0));
    }
    case CMD_RPM_DEINIT: {
        return app_hal_rpm_de_init();
    }
    case CMD_TILT_INIT: {
        return app_hal_tilt_init();
    }
    case CMD_TILT_DEINIT: {
        return app_hal_tilt_de_init();
    }
    case CMD_METER_PAGE: {
        return app_hal_meter_select_page(u(0));
    }
    case CMD_METER_CAL: {
        return app_hal_meter_calibrate_spl(f(0));
    }
    case CMD_METER_HOLD: {
        return app_hal_meter_hold(u(0));
    }
    case CMD_METER_VIEW: {
        return app_hal_meter_show_peak(u(0));
    }
    case CMD_METER_RESET: {
        return app_hal_meter_reset_peaks();
    }
    case CMD_TIMER_DOWN: {
        return app_hal_timer_countdown(u(0), u(1));
    }
    case CMD_TIMER_UP: {
        return app_hal_timer_countup();
    }
    case CMD_TIMER_PAUSE: {
        return app_hal_timer_pause();
    }
    case CMD_TIMER_RESUME: {
        return app_hal_timer_resume();
    }
    case CMD_TIMER_STOP: {
        return app_hal_timer_stop();
    }
    case CMD_TIMER_ACK: {
        return app_hal_timer_ack();
    }
    case CMD_CLOCK_SHOW: {
        return app_hal_clock_show();
    }
    case CMD_CLOCK_EDIT: {
        return app_hal_clock_edit_start();
    }
    case CMD_CLOCK_FINISH: {
        return app_hal_clock_edit_finish(u(0));
    }
    case CMD_COUNTER_START: {
        return app_hal_counter_start(NULL);
    }
    case CMD_COUNTER_STEP: {
        if (i(0) != -1 && i(0) != 1) return APP_ERR_ARG;
        return app_hal_counter_step(i(0));
    }
    case CMD_COUNTER_TARGET: {
        return app_hal_counter_set_target(i(0));
    }
    case CMD_PARAM_FINISH: {
        return app_hal_param_edit_finish(u(0));
    }
    case CMD_ALARM_DEFAULT: {
        return app_hal_alarm_enable_default(u(0));
    }
    case CMD_ALARM_SET: {
        return app_hal_alarm_configure(u(0), f(1), f(2), u(3));
    }
    case CMD_ALARM_OFF: {
        return app_hal_alarm_disable(u(0));
    }
    case CMD_ALARM_ACK: {
        return app_hal_alarm_ack(u(0));
    }
    case CMD_MIC_CAL: {
        return app_hal_mic_calibrate_spl(f(0));
    }
    case CMD_FAN_SET: {
        return app_hal_fan_configure(f(0), f(1), f(2), u(3));
    }
    case CMD_PROXIMITY_SET: {
        return app_hal_proximity_configure(u(0), u(1));
    }
    case CMD_PROXIMITY_SHOW: {
        return app_hal_proximity_show(u(0));
    }
    case CMD_PROXIMITY_RESET: {
        return app_hal_proximity_reset_count();
    }
    case CMD_NOISE_SET: {
        return app_hal_noise_set_threshold(f(0));
    }
    case CMD_NOISE_RESET: {
        return app_hal_noise_reset();
    }
    case CMD_RPM_SHOW: {
        return app_hal_rpm_show();
    }
    case CMD_TILT_SHOW: {
        return app_hal_tilt_show();
    }
    case CMD_TILT_SET: {
        return app_hal_tilt_configure(f(0), f(1), u(2));
    }
    case CMD_ADC_INIT: {
        return app_hal_wave_input_init(u(0), u(1), u(2), u(3));
    }
    case CMD_ADC_DEINIT: {
        return app_hal_wave_input_de_init();
    }
    case CMD_ADC_STREAM: {
        return app_hal_wave_stream_start(u(0), u(1), NULL, NULL);
    }
    case CMD_ADC_STOP: {
        return app_hal_wave_stream_stop();
    }
    case CMD_DAC_INIT: {
        return app_hal_wave_output_init(u(0), u(1));
    }
    case CMD_DAC_DEINIT: {
        return app_hal_wave_output_de_init();
    }
    case CMD_DAC_STOP: {
        return app_hal_wave_output_stop();
    }
    case CMD_LOOP_INIT: {
        return app_hal_wave_loopback_init(u(0), u(1), u(2), u(3), u(4));
    }
    case CMD_LOOP_DEINIT: {
        return app_hal_wave_loopback_de_init();
    }
    case CMD_CORE_INIT: {
        return app_hal_init();
    }
    case CMD_CORE_DEINIT: {
        return app_hal_de_init();
    }
    case CMD_COUNTER_CONFIG: {
        app_hal_counter_config_t cfg = {i(0), i(1), i(2), i(3), i(4), u(5), u(6)};
        return app_hal_counter_start(&cfg);
    }
    case CMD_PARAM_EDIT: {
        app_hal_param_range_t range = {i(1), i(2), i(3), u(4)};
        return app_hal_param_edit_start(i(0), &range);
    }
    case CMD_TIMER_GET: {
        app_hal_timer_status_t v;
        rc = app_hal_timer_get(&v);
        if (rc == APP_OK) {
            integer("state", v.state);
            unsigned_field("elapsed_ms", v.elapsed_ms);
            unsigned_field("remaining_ms", v.remaining_ms);
            unsigned_field("lap_ms", v.lap_ms);
            integer("countdown", v.countdown);
            integer("alarm_active", v.alarm_active);
            integer("has_lap", v.has_lap);
        }
        return rc;
    }
    case CMD_TIMER_LAP: {
        uint32_t v;
        rc = app_hal_stopwatch_lap(&v);
        if (rc == APP_OK) {
            unsigned_field("lap_ms", v);
        }
        return rc;
    }
    case CMD_METER_GET: {
        app_hal_meter_sample_t v;
        rc = app_hal_meter_get(u(0), &v);
        if (rc == APP_OK) {
            decimal("current", v.current);
            decimal("minimum", v.minimum);
            decimal("maximum", v.maximum);
            integer("has_current", v.has_current);
            integer("has_extrema", v.has_extrema);
            integer("held", v.held);
        }
        return rc;
    }
    case CMD_COUNTER_GET: {
        int32_t v;
        rc = app_hal_counter_get(&v);
        if (rc == APP_OK) {
            integer("value", v);
        }
        return rc;
    }
    case CMD_FAN_GET: {
        uint8_t v;
        rc = app_hal_fan_get_power(&v);
        if (rc == APP_OK) {
            integer("percent", v);
        }
        return rc;
    }
    case CMD_PROXIMITY_GET: {
        app_hal_proximity_state_t v;
        rc = app_hal_proximity_get(&v);
        if (rc == APP_OK) {
            integer("distance_mm", v.distance_mm);
            unsigned_field("passages", v.passages);
            integer("valid", v.valid);
            integer("occupied", v.occupied);
        }
        return rc;
    }
    case CMD_NOISE_GET: {
        app_hal_noise_state_t v;
        rc = app_hal_noise_get(&v);
        if (rc == APP_OK) {
            decimal("current_db_spl", v.current_db_spl);
            decimal("peak_db_spl", v.peak_db_spl);
            wide("exposure_ms=", v.exposure_ms);
            integer("valid", v.valid);
            integer("has_peak", v.has_peak);
        }
        return rc;
    }
    case CMD_RPM_GET: {
        app_hal_rpm_sample_t v;
        rc = app_hal_rpm_read(&v);
        if (rc == APP_OK) {
            decimal("rpm", v.rpm);
            integer("direction", v.direction);
            signed_wide("position", v.position);
        }
        return rc;
    }
    case CMD_TILT_GET: {
        app_hal_tilt_state_t v;
        rc = app_hal_tilt_get(&v);
        if (rc == APP_OK) {
            decimal("degrees", v.degrees);
            integer("valid", v.valid);
            integer("alarm", v.alarm);
        }
        return rc;
    }
    case CMD_ALARM_GET: {
        app_hal_alarm_state_t v;
        rc = app_hal_alarm_get(u(0), &v);
        if (rc == APP_OK) {
            decimal("value", v.value);
            integer("valid", v.valid);
            integer("triggered", v.triggered);
            integer("acknowledged", v.acknowledged);
        }
        return rc;
    }
    case CMD_DAC_GET: {
        uint8_t v;
        rc = app_hal_wave_output_running(&v);
        if (rc == APP_OK) {
            integer("running", v);
        }
        return rc;
    }
    case CMD_PARAM_GET: {
        app_hal_param_state_t state;
        int32_t value;
        rc = app_hal_param_get(&state, &value);
        if (rc == APP_OK) { integer("state", state); integer("value", value); }
        return rc;
    }
    case CMD_ADC_MEASURE: {
        app_hal_wave_report_t report;
        rc = app_hal_wave_measure(&report);
        if (rc == APP_OK) print_wave(&report);
        return rc;
    }
    case CMD_ADC_PULSE: {
        app_hal_wave_report_t report;
        rc = app_hal_pulse_measure(&report);
        if (rc == APP_OK) print_wave(&report);
        return rc;
    }
    case CMD_ADC_TRIGGER: {
        if (!u(3) || u(3) > sizeof capture / sizeof capture[0]) return APP_ERR_ARG;
        captured = 0;
        rc = app_hal_wave_capture_triggered(u(0), u(1), u(2), capture, u(3));
        if (rc == APP_OK) {
            captured = u(3);
            for (unsigned k = 0; k < captured; ++k) rt_kprintf("sample,%u,%u\n", k, (unsigned)capture[k]);
        }
        return rc;
    }
    case CMD_ADC_ANALYZE: {
        app_hal_wave_report_t report;
        if (!captured) return APP_ERR_STATE;
        rc = app_hal_wave_analyze(capture, captured, u(0), u(1), &report);
        if (rc == APP_OK) print_wave(&report);
        return rc;
    }
    case CMD_DAC_START: {
        app_hal_wave_signal_t signal = {u(0), u(1), u(2), u(3)};
        float hz;
        rc = app_hal_wave_output_start(&signal, &hz);
        if (rc == APP_OK) decimal("actual_hz", hz);
        return rc;
    }
    case CMD_DAC_BURST: {
        app_hal_wave_signal_t signal = {u(0), u(1), u(2), u(3)};
        float hz;
        rc = app_hal_wave_output_burst(&signal, u(4), &hz);
        if (rc == APP_OK) decimal("actual_hz", hz);
        return rc;
    }
    case CMD_LOOP_TEST: {
        app_hal_wave_signal_t signal = {u(0), u(1), u(2), u(3)};
        app_hal_wave_loopback_result_t result;
        rc = app_hal_wave_loopback_test(&signal, &result, NULL, NULL);
        if (rc == APP_OK) { print_wave(&result.measured); decimal("generated_hz", result.generated_hz); decimal("frequency_error_hz", result.frequency_error_hz); decimal("amplitude_error_mv", result.amplitude_error_mv); }
        return rc;
    }
    default: return APP_ERR_ARG;
    }
}
#undef u
#undef i
#undef f

/* Only the worker calls the HAL. MSH waits for the current reply while the worker
 * continues servicing timers, alarms and streams between commands. */
static struct rt_thread worker;
ALIGN(RT_ALIGN_SIZE) static rt_uint8_t stack[2304];
static struct rt_semaphore request, done;
static int started, busy;
static unsigned pending;
static number_t arguments[7];
static app_status_t reply, update_status;
static void serve(void *unused)
{
    (void)unused;
    rt_tick_t period = rt_tick_from_millisecond(APP_HAL_SERVICE_MS);
    int service = 0;
    for (;;) {
        int received = rt_sem_take(&request, service ? (rt_int32_t)(period ? period : 1) : RT_WAITING_FOREVER) == RT_EOK;
        if (received) {
            reply = dispatch(pending, arguments);
            if (reply == APP_OK) service = pending != CMD_CORE_DEINIT;
            /* Finish the reply line before update can emit asynchronous CSV. */
            if (reply == APP_OK) rt_kprintf("OK\n");
            else rt_kprintf("ERR %d\n", reply);
        }
        if (service) update_status = app_hal_update();
        if (received) rt_sem_release(&done);
    }
}
static int start_worker(void)
{
    if (started) return 1;
    rt_sem_init(&request, "hsm_req", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&done, "hsm_done", 0, RT_IPC_FLAG_FIFO);
    if (rt_thread_init(&worker, "hsm_hal", serve, RT_NULL, stack, sizeof stack,
                       FINSH_THREAD_PRIORITY, 10) == RT_EOK) {
        if (rt_thread_startup(&worker) == RT_EOK) { started = 1; return 1; }
        rt_thread_detach(&worker);
    }
    rt_sem_detach(&done);
    rt_sem_detach(&request);
    return 0;
}
static int hsm(int argc, char **argv)
{
    unsigned count = sizeof commands / sizeof commands[0];
    if (argc == 1 || !strcmp(argv[1], "help")) {
        const char *group = argc > 2 ? argv[2] : "";
        for (unsigned k = 0; k < count; ++k)
            if (!strncmp(commands[k].name, group, strlen(group)))
                rt_kprintf("hsm %s %s\n", commands[k].name, commands[k].usage ? commands[k].usage : "");
        rt_kprintf("hsm status\n");
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "status")) {
        rt_kprintf("worker=%d last_update=%d\n", started, update_status);
        return 0;
    }
    unsigned k = 0;
    while (k < count && strcmp(argv[1], commands[k].name)) ++k;
    if (k == count || (unsigned)(argc - 2) != strlen(commands[k].types)) {
        rt_kprintf("ERR -1: command or argument count; hsm help\n");
        return APP_ERR_ARG;
    }
    rt_enter_critical();
    if (busy) { rt_exit_critical(); rt_kprintf("ERR -2: busy\n"); return APP_ERR_BUSY; }
    busy = 1;
    rt_exit_critical();
    app_status_t rc = APP_ERR_ARG;
    for (int j = 0; j < argc - 2; ++j) {
        if (!parse(argv[j + 2], commands[k].types[j], &arguments[j])) {
            rt_kprintf("ERR %d: argument %d\n", rc, j + 1);
            goto release;
        }
    }
    rc = APP_ERR_STATE;
    if (start_worker()) {
        pending = k;
        rt_sem_release(&request);
        rt_sem_take(&done, RT_WAITING_FOREVER);
        rc = reply;
    } else {
        rt_kprintf("ERR %d\n", rc);
    }
release:
    rt_enter_critical();
    busy = 0;
    rt_exit_critical();
    return rc;
}
MSH_CMD_EXPORT(hsm, board APP_HAL commands);
