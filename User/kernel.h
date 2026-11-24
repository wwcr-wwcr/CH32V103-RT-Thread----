#ifndef __KERNEL_H
#define __KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "event.h"

typedef void (*evt_handler_t)(const message *ev);       //事件处理函数：main 里传的是 router_dispatch
typedef void (*render_cb_t)(void);      //渲染函数：每拍调一次，main 里传的是 router_render 

/**
 * @brief  开始运行事件
 * @param  on_evt   事件函数地址
 * @param  on_render   循环函数地址
 * @note   任何线程都能调，不阻塞：队列满了丢一个事件，返回就行
 */
void kernel_run(evt_handler_t on_evt, render_cb_t on_render);

/**
 * @brief  投一个事件
 * @param  type   0 = 按键，1 = 控件事件（见 event.h）
 * @param  code   按键号 / 控件编号
 * @param  value  控件当前值（按键事件传 0）
 * @note   任何线程都能调，不阻塞：队列满了丢一个事件，返回就行
 */
void kernel_post(uint8_t type, uint8_t code, int32_t value);

#ifdef __cplusplus
}
#endif

#endif
