/******************************************************************************
 * 文件名   : kernel.h
 * 说明     : 内核 —— 全工程只有 3 个接口：初始化、投事件、跑起来
 *
 *     kernel_init(处理函数, 渲染函数)   建队列，并把两个回调绑上
 *     kernel_post(类型, 编号, 数值)     投一个事件（任何线程都能调，不阻塞）
 *     kernel_run()                      起 kernel 线程进主循环，不返回
 *
 * 依赖方向（只向下，不回环）：
 *     kernel  ──>  event.h（事件结构）、lvgl、rt-thread
 *     kernel  ──×──>  app、task、ui 一个都不认识
 *
 * 事件是什么、谁来处理、处理完画什么，全部由外部决定：
 *     main 里 kernel_init(app_handle, app_render) 一绑，内核就只负责搬运。
 *
 * 线程模型：
 *     key 线程     中断驱动，投事件            HAL/KEY/key.c
 *     kernel 线程  本模块创建的：10ms 取事件 -> 处理函数 -> 渲染函数 -> LVGL 心跳
 *     task 线程    由 app.c 按需建删，跑任务的 update / on_key
 ******************************************************************************/

#ifndef __KERNEL_H
#define __KERNEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "event.h"

/* 事件处理函数：谁绑进来谁处理（一般是 app_handle） */
typedef void (*evt_handler_t)(const evt_t *ev);

/* 渲染函数：每拍调一次（一般是 app_render） */
typedef void (*render_cb_t)(void);

/**
 * @brief  初始化内核：建事件队列 + 绑定"谁来处理事件、谁来渲染"
 * @param  on_evt     事件处理函数，传 NULL 表示没人处理
 * @param  on_render  渲染函数，传 NULL 表示不需要刷新
 * @note   main 里最先调，按键线程才有地方投
 */
void kernel_init(evt_handler_t on_evt, render_cb_t on_render);

/**
 * @brief  投一个事件
 * @param  type   0 = 按键，1 = 控件事件（见 event.h）
 * @param  code   按键号 / 控件编号
 * @param  value  控件当前值（按键事件传 0）
 * @note   任何线程都能调，不阻塞：队列满了丢一个事件，返回就行
 */
void kernel_post(uint8_t type, uint8_t code, int32_t value);

/**
 * @brief  起 kernel 线程，进主循环，不返回
 * @note   要先 kernel_init()
 */
void kernel_run(void);

#ifdef __cplusplus
}
#endif

#endif
