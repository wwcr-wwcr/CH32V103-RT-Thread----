/******************************************************************************
 * 文件名   : tasks.h
 * 说明     : 任务实例的对外声明表
 *
 * 每个 task_x.c 里的 `const task_t task_x` 都在这里声明一次，
 * 需要用到任务指针的地方（main.c 的 router_init、menu.c 的 g_app_list）
 * 统一 include 这个头，不要在各处手抄 extern —— 改名/改类型时编译器能第一时间报错。
 *
 * 加一个新任务：在下面加一行 extern 即可。
 ******************************************************************************/

#ifndef __TASKS_H
#define __TASKS_H

#include "router.h"                 /* task_t 在这里定义 */

extern const task_t task_menu;      /* 首页任务，定义在 task/menu.c */

#endif
