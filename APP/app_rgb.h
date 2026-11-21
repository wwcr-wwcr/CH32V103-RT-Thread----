#ifndef APP_RGB_H
#define APP_RGB_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_RGB_OFF = 0, APP_RGB_RED = 1, APP_RGB_GREEN = 2, APP_RGB_YELLOW = 3,
    APP_RGB_BLUE = 4, APP_RGB_MAGENTA = 5, APP_RGB_CYAN = 6, APP_RGB_WHITE = 7
} app_rgb_color_t;

/* 初始化共阳 RGB 灯，初始为熄灭。 */
app_status_t app_rgb_init(void);
/* 熄灯并释放红、绿、蓝三路 GPIO。 */
app_status_t app_rgb_de_init(void);
/* 取消闪烁，立即设置颜色。color 取上表常量，APP_RGB_OFF 熄灯。 */
app_status_t app_rgb_set(app_rgb_color_t color);
/* 单色闪烁。color 选一种亮色，on_ms 为每次亮灯时长（>0），off_ms 为两次之间的
 * 熄灯时长（可为 0），count 为亮灯次数（>0）。调用立即返回，末次亮满后自动熄灯。
 * 单段时间向上取整到系统节拍，首段额外预留一拍补偿启动相位。正在闪烁时再次提交返回 BUSY。 */
app_status_t app_rgb_blink(app_rgb_color_t color, uint32_t on_ms, uint32_t off_ms, uint32_t count);
/* 双色交替闪烁。每一轮先显示 first 持续 first_ms，再显示 second 持续 second_ms。
 * 两色须不同且均为亮色，两段时长及轮数 count 均大于 0。立即返回；最后一轮两色
 * 均显示完毕后自动熄灭。可随时用 app_rgb_set() 指定常亮颜色或熄灯。 */
app_status_t app_rgb_blink_alternate(app_rgb_color_t first, uint32_t first_ms,
                                     app_rgb_color_t second, uint32_t second_ms, uint32_t count);
#ifdef __cplusplus
}
#endif
#endif
