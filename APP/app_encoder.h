#ifndef APP_ENCODER_H
#define APP_ENCODER_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int16_t delta;     /* 自初始化、清零或上次成功读取以来的增量，单位为四倍频计数。 */
    int64_t position;  /* 自初始化或清零以来的累计位置，单位同上。 */
} app_encoder_sample_t;

/* 初始化 P11 霍尔编码器并清零。无参数。使用 PA6/PA7、TIM3 正交计数。 */
app_status_t app_encoder_init(void);
/* 同时读取位移增量和累计位置。sample 指向结果，仅成功时更新。
 * 两次读取间的净位移绝对值须小于 32768 计数；由同一个采集任务按需读取。 */
app_status_t app_encoder_read(app_encoder_sample_t *sample);
/* 将当前轴位置设为零，并重新开始统计增量。无参数。 */
app_status_t app_encoder_zero(void);
/* 停止编码器并释放其引脚和定时器。无参数。 */
app_status_t app_encoder_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
