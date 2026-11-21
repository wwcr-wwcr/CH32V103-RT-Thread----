#ifndef APP_HAL_WAVE_OUTPUT_H
#define APP_HAL_WAVE_OUTPUT_H
#include "app_hal_wave_input.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef struct {
    app_hal_wave_kind_t kind;
    uint32_t frequency_hz;
    uint16_t amplitude_mvpp, offset_mv;
} app_hal_wave_signal_t;
/* DAC1=PA4（接功放），DAC2=PA5；vref_mv 为实际参考电压，0 用集中配置，各电压相对板地。 */
app_status_t app_hal_wave_output_init(uint8_t dac_channel, uint16_t vref_mv);
app_status_t app_hal_wave_output_de_init(void);
/* 异步循环输出。signal=NULL 使用集中配置的默认正弦波。频率单位 Hz、幅度为 mVpp。
 * 定时器按整数分频，actual_hz 可选。需 frequency_hz * OUTPUT_SAMPLES <= 100000。
 * 先 stop 再改变正在输出的参数。脉冲输出同样支持 signal=NULL。 */
app_status_t app_hal_wave_output_start(const app_hal_wave_signal_t *signal, float *actual_hz);
/* 连续输出恰好 cycles 周期，再保持 offset_mv。需 cycles * OUTPUT_SAMPLES + 1 <= DAC_CAPACITY。 */
app_status_t app_hal_wave_output_burst(const app_hal_wave_signal_t *signal, uint32_t cycles, float *actual_hz);
/* 查询硬件完成/错误并归还时基与 DMA；running=1 表示仍在输出。app_hal_update 也会自动回收。 */
app_status_t app_hal_wave_output_running(uint8_t *running);
app_status_t app_hal_wave_output_stop(void);
#ifdef __cplusplus
}
#endif
#endif
