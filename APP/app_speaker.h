#ifndef APP_SPEAKER_H
#define APP_SPEAKER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化扬声器 DAC 输出，使功放输入保持静音中点。 */
app_status_t app_speaker_init(void);
/* 静音并释放扬声器 DAC 通道。 */
app_status_t app_speaker_de_init(void);
/* 输出一个有符号 16 位单声道 PCM 样本。0 为静音中点；由调用方控制采样时刻。 */
app_status_t app_speaker_write_sample(int16_t sample);
/* 输出静音中点，保持设备就绪。 */
app_status_t app_speaker_mute(void);
/* 播放短 PCM 音频后静音。samples 是有符号 16 位单声道样本数组，count 为样本数，
 * rate_hz 为 1000~24000 Hz。线程内同步轮询，播放期间独占 TIM6；抢占会影响采样间隔。 */
app_status_t app_speaker_play_pcm(const int16_t *samples, size_t count, uint32_t rate_hz);
#ifdef __cplusplus
}
#endif
#endif
