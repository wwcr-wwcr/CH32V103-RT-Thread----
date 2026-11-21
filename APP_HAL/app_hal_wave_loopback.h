#ifndef APP_HAL_WAVE_LOOPBACK_H
#define APP_HAL_WAVE_LOOPBACK_H
#include "app_hal_wave_output.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    app_hal_wave_report_t measured;
    float generated_hz, frequency_error_hz, amplitude_error_mv;
} app_hal_wave_loopback_result_t;
/* 先将选用的 DAC 输出接到 ADC 输入，共地。该模块独占两个相应 HAL 接口直到释放。 */
app_status_t app_hal_wave_loopback_init(uint8_t dac, uint8_t adc, uint8_t channel, uint32_t rate_hz,
                                        uint16_t vref_mv);
app_status_t app_hal_wave_loopback_de_init(void);
/* 发出测试波、采集并停止，结果含测量与设定之差。无有效周期返回 NOT_FOUND。
 * 选填 write 输出一行误差 CSV；NULL 时输出至默认串口。 */
app_status_t app_hal_wave_loopback_test(const app_hal_wave_signal_t *signal, app_hal_wave_loopback_result_t *out,
                                         app_hal_wave_write_t write, void *context);
#ifdef __cplusplus
}
#endif
#endif
