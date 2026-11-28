/******************************************************************************
 * 文件名   : task_buzzer_ui.h
 * 说明     : 界面头文件 —— 空壳，等 LVGL 设计器生成后整段替换本文件
 *
 * render 现在没有参数。设计器生成的版本可能带参数（例如 float c），
 * 那时把 task_xxx.c 里 task_render() 的调用一并改成传参即可。
 ******************************************************************************/

#ifndef __TASK_BUZZER_UI_H
#define __TASK_BUZZER_UI_H

/* 建页 + 刷新：每帧调用，首次调用时把页面建出来 */
void task_buzzer_ui_render(void);

/* 关页：把 LVGL 对象还给堆 */
void task_buzzer_ui_destroy(void);

#endif
