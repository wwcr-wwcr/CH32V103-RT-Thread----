/* ============================================================
 * 文件名   : app_ui.h
 * 说明     : 由 LVGL 可视化设计器生成（基于 APP/common.h）
 * 屏幕     : 竖屏 320×480
 * 修改方式 : 重新在设计器里改，别手改这个文件
 * ============================================================ */

#ifndef __TASK_LED_UI_H
#define __TASK_LED_UI_H

#include <stdint.h>

#ifndef APP_UI_SCREEN_CNT
#define APP_UI_SCREEN_CNT  1
#endif

/**
 * @brief  渲染指定屏幕（首次调用会建页，之后只 load + 更新数据）
 * @param  screen  屏幕下标 0..0（对应：Screen1）
 * @param  c  绑定给控件的实时数据（float）
 * @param  is_down  绑定给控件的实时数据（uint8_t）
 */
void task_led_ui_render(uint8_t screen, float c, uint8_t is_down);

/**
 * @brief  销毁所有页面，把对象还给 LVGL 堆（关页时调用）
 */
void task_led_ui_destroy(void);

#endif