#ifndef APP_HAL_WAVE_INPUT_H
#define APP_HAL_WAVE_INPUT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef enum { APP_HAL_WAVE_UNKNOWN, APP_HAL_WAVE_SINE, APP_HAL_WAVE_TRIANGLE, APP_HAL_WAVE_SQUARE } app_hal_wave_kind_t;
typedef struct {
    app_hal_wave_kind_t kind;
    uint16_t min_code, max_code;
    uint8_t clipped, periodic;
    float peak_to_peak_mv, mean_mv, ac_rms_mv, frequency_hz, duty_percent, high_us, low_us;
} app_hal_wave_report_t;
typedef app_status_t (*app_hal_wave_write_t)(const char *data, size_t bytes, void *context);
/* adc=1/2，channel=0..15，rate_hz=2000..48000，须能从板载时钟整分频。vref_mv 为实测参考电压。
 * rate_hz 或 vref_mv 传 0，各自采用集中配置的默认值。 */
app_status_t app_hal_wave_input_init(uint8_t adc, uint8_t channel, uint32_t rate_hz, uint16_t vref_mv);
app_status_t app_hal_wave_input_de_init(void);
/* 采集同一帧计算各项数据。平直、样本不足或不规则的周期无有效频率；未知形状返回 UNKNOWN。 */
app_status_t app_hal_wave_measure(app_hal_wave_report_t *out);
/* 对所存 12 位等间隔样本作同样分析，可离线使用。 */
app_status_t app_hal_wave_analyze(const uint16_t *samples, size_t count, uint32_t rate_hz,
                                  uint16_t vref_mv, app_hal_wave_report_t *out);
/* 取有效周期及过中点的高低脉宽，找不到稳定周期则返回 NOT_FOUND。 */
app_status_t app_hal_pulse_measure(app_hal_wave_report_t *out);
/* 线程内软件帧触发。rising=1 上升沿，0 下降沿；threshold_mv，timeout_ms>0。
 * 返回从阈值后第一个样本开始的连续 count 点；count 不超过默认帧的一半。帧间有采集空隙。 */
app_status_t app_hal_wave_capture_triggered(uint16_t threshold_mv, uint8_t rising, uint32_t timeout_ms,
                                           uint16_t *out, size_t count);
/* 周期性输出 ASCII CSV。raw=1 另外输出各样本；write=NULL 走已打开的 RT 串口控制台。
 * interval_ms=0 用默认间隔。每次 update 最多采一帧并限量发送；帧间有空隙。
 * 回调返回 OK 表示已接受整行，BUSY 表示零字节已发送、需要重试，其他错误停止流。 */
app_status_t app_hal_wave_stream_start(uint32_t interval_ms, uint8_t raw, app_hal_wave_write_t write, void *context);
app_status_t app_hal_wave_stream_stop(void);
app_status_t app_hal_wave_input_poll(void);
#ifdef __cplusplus
}
#endif
#endif
