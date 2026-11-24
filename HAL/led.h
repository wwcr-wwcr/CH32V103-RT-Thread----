#ifndef BOARD_LED_H
#define BOARD_LED_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t led_color_t;  /* RGB 顺序为 0xRRGGBB。 */
enum {
    LED_COLOR_BLACK   = 0x000000,
    LED_COLOR_RED     = 0xff0000,
    LED_COLOR_GREEN   = 0x00ff00,
    LED_COLOR_BLUE    = 0x0000ff,
    LED_COLOR_YELLOW  = 0xffff00,
    LED_COLOR_MAGENTA = 0xff00ff,
    LED_COLOR_CYAN    = 0x00ffff,
    LED_COLOR_WHITE   = 0xffffff
};

/* task 中调用，首次自动配置 PWM。duty_percent 为 0..100，按比例缩放三路占空比。
 * 例：led_set(LED_COLOR_RED, 30); 或 led_set(0xff8000, 50);
 * 占空比为 0 或颜色为黑色时熄灯。成功返回 0，参数无效返回负数。 */
int led_set(led_color_t color, unsigned duty_percent);

#ifdef __cplusplus
}
#endif
#endif
