/******************************************************************************
 * 文件名   : kernel.c
 * 说明     : 内核：队列 -> 事件处理函数 -> 渲染函数 -> LVGL 心跳
 *
 * 一拍（10ms）就四步，多一步都没有：
 *     1. 从队列取一个事件，交给 bind 进来的处理函数（没有就跳过）
 *     2. 调 bind 进来的渲染函数（界面的文字/曲线在这刷新）
 *     3. lv_timer_handler()：LVGL 刷新、内部定时器、触摸扫描
 *
 * 为什么事件只取一个：保证每拍都回到渲染和触摸扫描，
 * 一次来 8 个键也只处理 1 个，剩下的下一拍接着来，界面不会卡。
 ******************************************************************************/

#include "kernel.h"
#include "rtthread.h"
#include "lvgl/lvgl.h"
#include "app.h"
/**************************************************************************************************
 *                                        内部状态
 **************************************************************************************************/
/* 事件队列：key 线程 / 触摸回调 投进来，kernel 线程取出去。
 * 深度 8，满了只丢一个事件，谁也不等谁。 */
static rt_mq_t s_q = RT_NULL;

/* 谁处理事件、谁负责渲染 —— 由 main 里 kernel_init() 一起传进来 */
static evt_handler_t s_on_evt    = RT_NULL;
static render_cb_t   s_on_render = RT_NULL;

/* kernel 线程：栈静态分配，不占堆。
 * 6144 是实测够用的深度（LVGL 渲染 + 页面回调 + 音频那几层）。 */
static struct rt_thread s_tid;
static rt_uint8_t s_stack[6144];

/**************************************************************************************************
 *                                        对外接口
 **************************************************************************************************/
void kernel_init(evt_handler_t on_evt, render_cb_t on_render)
{
    s_on_evt    = on_evt;
    s_on_render = on_render;
    s_q = rt_mq_create("evt", sizeof(evt_t), 8, RT_IPC_FLAG_FIFO);
}

void kernel_post(uint8_t type, uint8_t code, int32_t value)
{
    evt_t ev;
    ev.type  = type;
    ev.code  = code;
    ev.value = value;
    rt_mq_send(s_q, &ev, sizeof(ev));
}
/**************************************************************************************************
 *                                        主循环
 **************************************************************************************************/
static void kernel_entry(void *parameter)
{
    evt_t ev;

    (void)parameter;

    for(;;)
    {
        /* 1. 等事件，最多 10ms。10ms 是全系统节拍，任务的 update 也按它跑 */
        if(rt_mq_recv(s_q, &ev, sizeof(ev),
                      rt_tick_from_millisecond(10)) == RT_EOK)
        {
            if(s_on_evt != RT_NULL)
            {
                s_on_evt(&ev);              /* 交给绑进来的处理函数 */
            }
        }

        /* 2. 刷新：调用方负责切页检查 + 各页面的 render */
        if(s_on_render != RT_NULL)
        {
            s_on_render();
        }

        /* 3. LVGL 心跳：刷新、内部定时器、触摸扫描都靠它 */
        lv_timer_handler();
    }
}

void kernel_run(void)
{
    s_q = rt_mq_create("evt", sizeof(evt_t), 8, RT_IPC_FLAG_FIFO);

    /* 9 = 优先级（比任务高，刷新不被抢）；10 = 时间片 */
    if(rt_thread_init(&s_tid, "kernel", kernel_entry, RT_NULL,
                      s_stack, sizeof(s_stack), 9, 10) != RT_EOK)
    {
        rt_kprintf("kernel: thread init failed\r\n");
        return;
    }

    rt_thread_startup(&s_tid);
}

/**************************************************************************************************
 *                                        MSH 调试
 **************************************************************************************************/
static void kernel_cmd(void)
{
    
}
MSH_CMD_EXPORT_ALIAS(kernel_cmd, kernel, show kernel loop counter);
