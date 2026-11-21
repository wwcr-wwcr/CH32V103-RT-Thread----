#ifndef APP_VOICE_H
#define APP_VOICE_H

#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ASRPRO 配套固件的设备与动作码；自定义固件也可传其约定的字节值。 */
typedef enum {
    APP_VOICE_MODULE, APP_VOICE_KEY1, APP_VOICE_KEY2, APP_VOICE_KEY3, APP_VOICE_KEY4,
    APP_VOICE_LED, APP_VOICE_FAN, APP_VOICE_HEATER, APP_VOICE_LCD,
    APP_VOICE_DISTANCE, APP_VOICE_ATTITUDE, APP_VOICE_LIGHT, APP_VOICE_AUDIO = 0xff
} app_voice_target_t;
typedef enum {
    APP_VOICE_OFF, APP_VOICE_ON, APP_VOICE_DECREASE, APP_VOICE_INCREASE,
    APP_VOICE_QUERY, APP_VOICE_TARGET_TEMPERATURE, APP_VOICE_CURRENT_TEMPERATURE
} app_voice_action_t;
typedef struct { uint8_t target, action; } app_voice_command_t;
/* 初始化板载语音接口，9600/8N1，后台接收并缓存。 */
app_status_t app_voice_init(void);
/* 停止接收并清除半帧。 */
app_status_t app_voice_de_init(void);
/* 给模块发一条完整命令。target 和 action 由模块词条固件决定。 */
app_status_t app_voice_send(uint8_t target, uint8_t action);
/* 非阻塞取一条识别指令；尚无完整帧为 BUSY，线路或缓冲出错为 IO。 */
app_status_t app_voice_receive(app_voice_command_t *command);

#ifdef __cplusplus
}
#endif
#endif
