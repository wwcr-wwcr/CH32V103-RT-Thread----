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
/* 8 位风格"方波/脉冲"音，duty 为高电平占比 10..90（30~50 最像 FC 音源）。
 * duration_ms 传 0 = 一直响，直到 speaker_stop() 或播下一个音（做音乐用这个）。 */
int speaker_square(uint32_t hz, uint8_t volume_percent, uint8_t duty_percent, uint32_t duration_ms);
int speaker_stop(void);
/* 单声道有符号 16 位 PCM，按 1000..48000 Hz 播放，播完返回。 */
int speaker_play(const int16_t *samples, size_t count, uint32_t rate);
/* 直接播 12 位 DAC 码值（0~4095，2048 = 中点）：麦克风采集的 uint16 数组就是这个格式，
 * 不换算、不减直流，波形 1:1 还原。例：speaker_play_code(s_smp, 4096, 16000); */
int speaker_play_code(const uint16_t *codes, size_t count, uint32_t rate);

#ifdef __cplusplus
}
#endif
#endif
