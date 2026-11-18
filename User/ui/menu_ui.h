/******************************************************************************
 * 文件名   : menu_ui.h
 * 说明     : 选择页界面（menu.c 只需要调下面这几个函数）
 ******************************************************************************/

#ifndef __MENU_UI_H
#define __MENU_UI_H

#include <stdint.h>

/**
 * @brief  注册触摸按钮回调（左箭头 / 右箭头 / 卡片）
 * @note   回调在 ui 线程里被调用，实现里只做"投一个键"这种快操作
 */
void menu_ui_set_tap_cb(void (*on_prev)(void), void (*on_next)(void), void (*on_enter)(void));

/**
 * @brief  画出（或更新）选择页
 * @param  index  当前选中的任务序号（0 开始）
 * @param  total  任务总数
 * @param  name   当前任务名
 * @note   index 没变就直接返回，不会白重绘
 */
void menu_ui_render(uint8_t index, uint8_t total, const char *name);

/**
 * @brief  销毁选择页，把内存还给 LVGL 堆
 * @note   只能在 ui 线程调，而且调用前页面必须已经不在前台
 */
void menu_ui_destroy(void);

#endif
