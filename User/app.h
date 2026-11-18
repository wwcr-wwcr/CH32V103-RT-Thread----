/******************************************************************************
 * 文件名   : app.h
 * 说明     : 应用（一个界面 + 一个线程）的约定 + 应用管理器
 *
 * 全模块只有 4 个接口：
 *
 *     app_init(首页)        记首页（队列跟着任务走，不归本模块管）
 *     app_handle(事件)      kernel 每收到一个事件就调它：分给当前任务的 on_key / on_set
 *     app_render()          每拍调一次：先切页（如果请求过），再渲染当前页面
 *     app_request(应用)     请求切到某个应用；传 NULL 表示回首页
 *
 * 依赖方向：
 *     app  ──>  event.h、key.h、rt-thread
 *     app  ──×──> 不回依赖 kernel：事件是 kernel 调进来的，不是 app 去要的
 *
 * 一层一层看下来：
 *     kernel  取事件 -> app_handle()；每拍 -> app_render()
 *     app     按 type 把事件送进"当前任务线程"，切页也在这一层做
 *     task    只写自己的 setup/update/on_key/on_set/render，不碰线程
 *     ui      只画界面，不碰业务（所有 lv_ 都从 render 进来）
 *
 * 新增界面三步：
 *     1. 在 task/ 下写一个 task_xxx.c（填下面这张表）
 *     2. 在 ui/ 下写它的 xxx_ui.c（只画界面）
 *     3. 到 task/menu.c 里 extern 一行 + g_app_list[] 加一行
 ******************************************************************************/

#ifndef __APP_H
#define __APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "event.h"
#include "key.h"

/**
 * @brief 一个应用（一个界面 + 一个线程）的描述
 *
 * name       标题栏显示的名字，同时也是线程名（建议 <= 7 个字符）
 * stack_size 线程栈大小（字节）。只画字符 1024 够；有浮点/大字符串给 2048
 * priority   线程优先级，数字越大优先级越低（key=8、kernel=9，任务建议 12 左右）
 * setup      线程启动后调用：初始化硬件、画内容区（y >= UI_CONTENT_Y）
 * update     周期刷新（10ms 一次），传 NULL 表示静态界面，线程只等事件不空转
 * on_key     本界面的按键。只会收到 K1/K2/K3；K4 是全局退出键，永远不下发给任务
 * on_set     触摸控件事件。只会收到滑条 / 按钮发来的 id + value
 * render     渲染回调，在 kernel 线程里被周期调用，里面只调一个 xxx_ui_render 即可。
 *            所有 lv_ 调用只能出现在 render 链路里，任务线程绝对不许碰 lv_。
 *            切页时 app_render() 会先主动调一次，用来把新页面搭出来。
 * teardown   线程退出前调用，释放本任务占用的资源（跑在任务线程里）
 * ui_destroy 退页时调用，把本任务的页面还给 LVGL 堆（跑在 kernel 线程里）
 *
 * 队列：一个任务一条
 *     队列在任务线程起来时创建、线程退出时删除，任务之间不可能收到对方的事件。
 *     当前策略仍是"退页就删线程"，所以同一时刻只有一个任务线程活着；
 *     想做后台常驻（时钟、倒计时），把 app.c 里的 s_host 改成数组即可，
 *     队列已经是每个任务独立的，不用再动别的地方。
 */
typedef struct
{
    const char *name;
    uint16_t    stack_size;
    uint8_t     priority;

    void (*setup)(void);            /* 逻辑线程：初始化硬件 */
    void (*update)(void);           /* 逻辑线程：周期处理（可 NULL） */
    void (*on_key)(key_id_t key);   /* 逻辑线程：按键（可 NULL） */
    void (*on_set)(uint8_t id, int32_t value);  /* 逻辑线程：触摸控件事件（可 NULL） */
    void (*render)(void);           /* kernel 线程：渲染（可 NULL），内部只调一个 xxx_ui_render */
    void (*teardown)(void);         /* 逻辑线程：清理（可 NULL） */
    void (*ui_destroy)(void);       /* kernel 线程：销毁本任务的页面，归还 LVGL 堆 */
} app_task_t;

/**
 * @brief  初始化应用层：记下首页
 * @param  home  上电后第一个界面，一般传 &task_menu
 * @note   只记指针，不建线程也不画界面 —— 首页由第一拍 app_render() 拉起来。
 *         任务队列跟着线程走，这里不管。
 */
void app_init(const app_task_t *home);

/**
 * @brief  事件入口：分发给当前任务线程
 * @param  ev  event.h 里的事件
 * @note   跑在 kernel 线程里，禁止阻塞：这里只做"写一个指针 / 投一条消息"。
 *         type=0 按键 -> on_key（K4 在这里拦下，回首页）
 *         type=1 控件 -> on_set
 */
void app_handle(const evt_t *ev);

/**
 * @brief  每拍调用：先处理切页请求，再渲染当前页面
 * @note   只有 kernel 线程能调
 */
void app_render(void);

/**
 * @brief  请求切到某个应用
 * @param  app  目标应用的地址；传 NULL 表示回首页（任务里做"退出"按钮就传 NULL）
 * @note   任何线程都能调：只写一个指针，真正的切换在下一拍 app_render() 里做
 */
void app_request(const app_task_t *app);

#ifdef __cplusplus
}
#endif

#endif
