#ifndef BOARD_SPEAKER_H
#define BOARD_SPEAKER_H
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif
/* 在线程中播放 100..6000 Hz 音调，音量 0..100%。时长 0 表示持续播放。
 * 例：speaker_tone(440, 35, 250); */
int speaker_tone(uint32_t hz, uint8_t volume_percent, uint32_t duration_ms);
int speaker_stop(void);
/* 单声道有符号 16 位 PCM，按 1000..48000 Hz 播放，播完返回。 */
int speaker_play(const int16_t *samples, size_t count, uint32_t rate);

#ifdef __cplusplus
}
#endif
#endif
