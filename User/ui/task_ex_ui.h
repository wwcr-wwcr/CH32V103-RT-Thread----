#ifndef __X_UI_H
#define __X_UI_H

#include <stdint.h>

/* 【必须】两个函数名和签名由 task 那边决定，改了要同步 task_x.c */
void x_ui_render(/* 【你的】参数 */);
void x_ui_destroy(void);

#endif
