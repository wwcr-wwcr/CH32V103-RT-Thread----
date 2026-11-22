/******************************************************************************
 * 文件名   : menu_ui.h
 * 说明     : 选择页界面 —— 一屏按钮列，点哪个进哪个
 *
 * 只有 2 个接口：
 *     menu_ui_render(名字表, 个数, 选中项)   画 / 更新页面
 *     menu_ui_destroy()                     把页面还给 LVGL 堆
 *
 * 按钮是 lv_obj_create 出来的圆角卡片，点击时发一个 type=1 的控件事件，
 * 编号 = 下标 + 1；menu.c 收到后直接 router_go()。
 ******************************************************************************/

#ifndef __MENU_UI_H
#define __MENU_UI_H

#include <stdint.h>

/* 控件事件编号：按钮 1..N（下标+1），滑条固定 200，避开按钮 */
#define MENU_ID_BRIGHT  200

/**
 * @brief  画出（或更新）选择页
 * @param  names  任务名表，按显示顺序排
 * @param  count  任务个数（一屏最多 8 个：2 列 × 4 行）
 * @param  sel    当前选中项（键盘操作时的高亮），0 开始
 * @note   第一次调用搭页面，之后只更新高亮；sel 没变就什么都不做
 */
void menu_ui_render(const char **names, uint8_t count, uint8_t sel);

/**
 * @brief  销毁选择页，把内存还给 LVGL 堆
 * @note   只能在 kernel 线程调，而且调用前页面必须已经不在前台
 */
void menu_ui_destroy(void);

#endif
