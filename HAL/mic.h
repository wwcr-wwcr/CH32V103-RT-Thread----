#ifndef MIC_H
#define MIC_H
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* PA1 麦克风，单点返回 12 位原始码值。 */
int mic_read(void);
/* DMA 采集 count 点，rate_hz 为 0 表示全速。完成返回 0。 */
int mic_capture(uint16_t *samples, size_t count, uint32_t rate_hz);
/* 采集同样的一帧并计算峰峰值。帧仍可用于 FFT，负数表示失败。 */
int mic_level(uint16_t *samples, size_t count, uint32_t rate_hz);

#ifdef __cplusplus
}
#endif
#endif
