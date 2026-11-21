#ifndef BSP_DAC_H
#define BSP_DAC_H
#include "bsp_types.h"

/* 用途：开启 12 位 DAC 通道，初始码值为 0。参数：channel 为 1（PA4）或 2（PA5）。
 * 注意：通道独立，输出缓冲开启；PA4 接板载 SPK_DAC。 */
bsp_status_t bsp_dac_init(uint8_t channel);
/* 用途：设置模拟输出。参数：channel 为 1 或 2，code 为 0~4095。
 * 注意：实际输出电压取决于 VREF+、负载与稳定时间。 */
bsp_status_t bsp_dac_write(uint8_t channel, uint16_t code);
/* 用途：关闭并释放一个通道。参数：channel 同上。注意：其余通道保持当前输出。 */
bsp_status_t bsp_dac_de_init(uint8_t channel);
/* 借用 TIM7_UP + DMA2_CH4，以实际 sample_hz 依次写入 DAC。数组须在片内 SRAM，
 * 保持不变直到 stop。circular=0 正常传输到末值，1 为循环；空闲输出由 stop 设置。 */
bsp_status_t bsp_dac_playback_start(uint8_t channel, const uint16_t *table, uint16_t count,
                                     uint32_t sample_hz, uint8_t circular, float *actual_hz);
bsp_status_t bsp_dac_playback_running(uint8_t channel, uint8_t *running);
bsp_status_t bsp_dac_playback_stop(uint8_t channel, uint16_t idle);
#endif
