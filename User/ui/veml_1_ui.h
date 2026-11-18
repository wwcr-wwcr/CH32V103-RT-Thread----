#ifndef __TASK_1_UI_H
#define __TASK_1_UI_H

#include "common.h"

/* 三个参数就是这一帧要显示的东西，由 task_1.c 的 render 传入 */
void custom_ui_render(uint16_t lux, uint8_t bright, uint8_t led_on);
void custom_ui_destroy(void);

#endif
