#ifndef BOARD_ADC_H
#define BOARD_ADC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef enum { ADC_PA1 = 1, ADC_PB0 = 8, ADC_PB1 = 9 } adc_input_t;
/* 在线程中读取原始 12 位码值，负数表示失败。板上 ADC 输入为 PB1。 */
int adc_read(adc_input_t input);
/* rate_hz=0 对应的标称采样率（点/秒），用于计算频率轴。 */
uint32_t adc_max_rate_hz(void);
/* DMA 直接写入调用方 SRAM，1..65535 点，完成返回 0。
 * rate_hz 为定时采样率；传 0 表示最短采样窗口、自由连续转换。
 * 单帧应位于同一 64 KiB 地址区间（部分 V307 的 DMA1 限制）。例如：
 * static uint16_t samples[1024] __attribute__((aligned(2048)));
 * adc_capture(ADC_PB1, samples, 1024, 200000); */
int adc_capture(adc_input_t input, uint16_t *samples, size_t count, uint32_t rate_hz);

#ifdef __cplusplus
}
#endif
#endif
