#ifndef BOARD_DAC_H
#define BOARD_DAC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* task 的外部电压输出固定在 PA5。码值 0..4095。 */
int dac_write(uint16_t code);
int dac_stop(void);
/* 定时器触发 DMA 循环输出；rate_hz 为每秒样点数，范围 1..1000000。
 * 调用方保证表中为 12 位码值，SRAM 中的表在停止或替换前保持有效。
 * 输出基频 = rate_hz / count。 */
int dac_play(const uint16_t *wave, size_t count, uint32_t rate_hz);

/* DAC1 / PA4 供 HAL 扬声器驱动独占，在持有扬声器锁时调用。 */
int dac1_write(uint16_t code);
int dac1_play(const uint16_t *wave, uint16_t count, uint32_t rate_hz);

#ifdef __cplusplus
}
#endif
#endif
