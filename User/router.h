/******************************************************************************
 * 文件名   : router.h
 * 说明     : 页面路由 —— 只决定"现在跑哪个任务"，不碰线程、不碰队列
 *
 * 任务完全自治：
 *     线程自己建（rt_thread_create 里传的就是任务自己的函数）、队列自己建、
 *     退出握手自己也自己实现。本模块不知道这些存在，只调 4 个回调：
 *
 *         task->open()   打开：任务自己建线程 + 建队列 + 启动
 *         task->close()  关闭：任务自己发退出 + 等收尾 + 删队列
 *         task->post()   投递：任务自己把事件塞进自己的队列
 *         task->render() 渲染：前台每拍画一帧（只有它能碰 lv_）
 *
 * 对外 4 个接口：
 *     router_init(首页)      记首页
 *     router_go(任务)        切到某个任务；传 NULL = 回首页
 *     router_dispatch(消息)  kernel 的事件入口（K4 回首页在这里拦）
 *     router_render()        kernel 的渲染入口（切页 + 前台 render）
 *
 * 两条约定：
 *     1. open() 和 close() 必须可重入（重复调不出事）：反复进出同一个任务时
 *        路由层只管调，任务自己用 if(s_tid != RT_NULL) 挡住。
 *     2. 切页顺序由路由层保证：开新的 -> 画新页（成为当前屏）-> 关旧的 -> 删旧页面。
 *        LVGL 不许删正在显示的 screen，这个顺序不能换，所以集中在这里。
 ******************************************************************************/

#ifndef __ROUTER_H
#define __ROUTER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "event.h"

/**
 * @brief 一个任务（一个界面 + 一条它自己的线程）的描述
 *
 * name       任务名：调试打印用，也当线程名（建议 <= 7 字符）
 * keep       0 = 切走就 close()；1 = 常驻，切走后线程继续跑自己的循环
 * open       打开：自己建队列 + 建线程 + rt_thread_startup（要能重复调）
 * close      关闭：自己投退出、等线程收尾、删队列（要能重复调）
 * post       投递：把事件放进自己的队列（跑在 kernel 线程，不许阻塞）
 * render     前台每拍调一次，所有 lv_ 都从这进（跑在 kernel 线程）
 * ui_destroy 关页时把本任务的页面还给 LVGL 堆（跑在 kernel 线程）
 *
 * 栈大小、优先级这些由任务在 open() 里自己写死，路由层不关心。
 */
typedef struct
{
    const char *name;
    uint8_t     keep;

    void (*open)(void);
    void (*close)(void);
    void (*post)(const message *msg);
    void (*render)(void);
    void (*ui_destroy)(void);
} task_t;

/**
 * @brief  初始化路由：记下首页（不打开任何线程）
 * @param  home  上电后第一个界面，一般传 &task_menu
 * @note   首页由第一拍 router_render() 打开
 */
void router_init(const task_t *home);

/**
 * @brief  切到某个任务
 * @param  task  目标任务；传 NULL 表示回首页
 * @note   任何线程都能调：只写一个指针，真正的切换在下一拍 router_render() 里做
 */
void router_go(const task_t *task);

/**
 * @brief  事件入口：交给前台任务的 post()
 * @param  msg  event.h 里的消息
 */
void router_dispatch(const message *msg);

/**
 * @brief  渲染入口：先处理切页请求，再调前台任务的 render()
 * @note   只有 kernel 线程能调
 */
void router_render(void);

#ifdef __cplusplus
}
#endif

#endif
