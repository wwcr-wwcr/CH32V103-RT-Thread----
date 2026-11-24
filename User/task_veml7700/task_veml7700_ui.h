/* ============================================================
 * 文件名   : task_veml7700.h
 * 说明     : 由 LVGL 可视化设计器生成（基于 APP/LVGL_DEMO/LVGL_Demo.h）
 * 屏幕     : 竖屏 320×480
 * 修改方式 : 重新在设计器里改，别手改这个文件
 * ============================================================ */

#ifndef __TASK_VEML7700_H
#define __TASK_VEML7700_H

#include <stdint.h>

/**
 * @brief  渲染页面（首次调用会建页，之后只 load + 更新数据）
 * @param  c  绑定给控件的实时数据（float）
 */
void task_veml7700_render(float c);

/**
 * @brief  销毁所有页面，把对象还给 LVGL 堆（关页时调用）
 */
void task_veml7700_destroy(void);

#endif