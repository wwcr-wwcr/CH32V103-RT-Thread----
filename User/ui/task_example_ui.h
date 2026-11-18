/******************************************************************************
 * 文件名   : task_example_ui.h
 * 说明     : EXAMPLE 页面（纯软件演示：计数 + 状态 + 操作次数）
 *
 * 只有 2 个接口，任务侧传一帧要显示的东西进来即可：
 *     demo_ui_render(计数, 是否在跑, 操作次数)
 *     demo_ui_destroy()
 ******************************************************************************/

#ifndef __TASK_EXAMPLE_UI_H
#define __TASK_EXAMPLE_UI_H

#include <stdint.h>

/**
 * @brief  渲染一帧
 * @param  count   计数器当前值
 * @param  run     1 = 在计数，0 = 暂停
 * @param  events  按键 / 触摸总共按了多少下
 */
void demo_ui_render(uint32_t count, uint8_t run, uint16_t events);

/**
 * @brief  销毁页面，把内存还给 LVGL 堆
 * @note   只能在 kernel 线程调，调用前页面必须已经不在前台
 */
void demo_ui_destroy(void);

#endif
