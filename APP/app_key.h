#ifndef APP_KEY_H
#define APP_KEY_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum { APP_KEY_1 = 1, APP_KEY_2, APP_KEY_3, APP_KEY_4 } app_key_id_t;
enum {
    APP_KEY_EVENT_CLICK = 1U, APP_KEY_EVENT_DOUBLE_CLICK = 2U,
    APP_KEY_EVENT_LONG_PRESS = 4U
};

/* 初始化四个按键并每 5 ms 自动扫描；上电时已按住的键在首次松开前不产生手势。 */
app_status_t app_key_init(void);
/* 释放普通 APP 的按键使用权并清除旧手势。还有导航订阅时保留扫描，最后一次释放时关闭 GPIO。 */
app_status_t app_key_de_init(void);
/* 菜单输入订阅。同一组 GPIO 和扫描定时器由两个使用者共用到最后一次释放。
 * 普通 app_key_init 成功后，导航订阅暂时读取到零事件；普通使用者释放后自动恢复。
 * 所有权转换会丢弃旧手势及已按住的键，避免一次按压操作两个页面。 */
app_status_t app_key_navigation_init(void);
app_status_t app_key_navigation_take_events(app_key_id_t key, uint8_t *events);
app_status_t app_key_navigation_de_init(void);
/* 主动追加一次扫描，一般只需读状态或取事件。消抖 20 ms，长按 700 ms，
 * 第一次松开后 300 ms 内再次按下可形成双击。 */
app_status_t app_key_scan(void);
/* 读取消抖后的按键状态。key 取 APP_KEY_1~4，pressed 返回 1 表示按下。 */
app_status_t app_key_is_pressed(app_key_id_t key, uint8_t *pressed);
/* 读取并清除该键的手势位。无事件返回 0 位，同类型多次发生会合并。 */
app_status_t app_key_take_events(app_key_id_t key, uint8_t *events);
/* 分别判断并取走指定手势。key 为 APP_KEY_1~4；成功时 detected 为 1（发生）或 0。
 * 仅消费本键的这一类事件，其他手势留给后续调用。与 take_events 共用同一份事件。
 * 短按在双击窗口结束后确认，双击在第二次松开后确认，长按达门限后只报告一次。 */
app_status_t app_key_take_short_press(app_key_id_t key, uint8_t *detected);
app_status_t app_key_take_double_click(app_key_id_t key, uint8_t *detected);
app_status_t app_key_take_long_press(app_key_id_t key, uint8_t *detected);
#ifdef __cplusplus
}
#endif
#endif
