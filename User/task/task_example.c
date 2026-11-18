/******************************************************************************
 * 文件名   : task_example.c
 * 说明     : EXAMPLE —— 模板任务（纯软件演示，不碰任何硬件）
 *
 * 一个任务的完整样子（本文件和 menu.c 结构完全一致）：
 *     ① 数据      本线程私有状态
 *     ② 快照      给 render 读的一份拷贝（关中断拷，避免读到一半被改）
 *     ③ 资源      s_exit / s_q / s_tid：open 建，close 删
 *     ④ 逻辑      周期活 demo_tick() + 事件 demo_key() / demo_set()
 *     ⑤ 线程本体  demo_thread()：rt_thread_create 的入口就是它
 *     ⑥ 生命周期  open / close / post + render + 任务描述
 *
 * 演示：每 100ms 计一次数
 *     K1 暂停 / 继续      K2 +10      K3 清零
 *     触摸 +10 / CLR / QUIT（QUIT = 回首页，等价于按 K4）
 *
 * 换成真实业务时，只改 ④ 里那三个函数，其余原样保留。
 ******************************************************************************/

#include "router.h"
#include "key.h"
#include "ui/task_example_ui.h"
#include "rtthread.h"
#include "rthw.h"

/**************************************************************************************************
 *                                        ① 数据（只有本线程写）
 **************************************************************************************************/
static uint32_t s_count  = 0;       /* 计数器 */
static uint8_t  s_run    = 1;       /* 1 = 在计数 */
static uint16_t s_events = 0;       /* 按了多少下（按键 + 触摸） */

/**************************************************************************************************
 *                                        ② 快照（本线程写 / render 读）
 **************************************************************************************************/
typedef struct
{
    uint32_t count;
    uint8_t  run;
    uint16_t events;
} demo_view_t;

static demo_view_t s_view;

/**
 * @brief  把当前状态拷一份给渲染用（关中断，防止渲染读到一半被改）
 */
static void view_push(void)
{
    demo_view_t v;
    rt_base_t   level;

    v.count  = s_count;
    v.run    = s_run;
    v.events = s_events;

    level = rt_hw_interrupt_disable();
    s_view = v;
    rt_hw_interrupt_enable(level);
}

/**************************************************************************************************
 *                                        ③ 资源（open 建 / close 删）
 **************************************************************************************************/
static rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */
static rt_mq_t     s_q    = RT_NULL;    /* 我自己的队列 */
static rt_thread_t s_tid  = RT_NULL;    /* 我自己的线程 */

/**************************************************************************************************
 *                                        ④ 逻辑（跑在自己的线程里）
 **************************************************************************************************/
/**
 * @brief  周期活：每拍调用一次
 * @note   换成真实业务时，采样、喂音频、刷数码管都写这里
 */
static void demo_tick(void)
{
    if(s_run != 0)
    {
        s_count++;
        view_push();
    }
}

/**
 * @brief  实体按键
 * @param  key  K1~K3（K4 被 router 拦下）；长按 5~8、双击 9~12 也在这里收
 */
static void demo_key(key_id_t key)
{
    switch(key)
    {
        case KEY_ID_1:                          /* K1 暂停 / 继续 */
            s_run = (uint8_t)((s_run == 0) ? 1 : 0);
            break;

        case KEY_ID_2:                          /* K2 +10 */
            s_count += 10;
            break;

        case KEY_ID_3:                          /* K3 清零 */
            s_count = 0;
            break;

        default:                                /* 长按 / 双击暂不处理 */
            return;
    }

    s_events++;
    view_push();
}

/**
 * @brief  触摸控件事件
 * @param  id     控件编号，见 ui/task_example_ui.c（1 +10 / 2 CLR / 3 QUIT）
 * @param  value  控件给的数值（按钮用不上）
 */
static void demo_set(uint8_t id, int32_t value)
{
    (void)value;

    switch(id)
    {
        case 1:
            s_count += 10;
            break;

        case 2:
            s_count = 0;
            break;

        case 3:
            /* 只写一个指针，不阻塞。真正的关线程 + 删队列由 router 在
             * kernel 线程里做（它调 demo_close），这里不能自己 close 自己。 */
            router_go(RT_NULL);
            return;

        default:
            return;
    }

    s_events++;
    view_push();
}

/**************************************************************************************************
 *                                        ⑤ 线程本体
 **************************************************************************************************/
/**
 * @brief  线程入口：rt_thread_create 直接用的就是这个函数
 * @param  arg  没用到
 * @note   100ms 一拍；没有事件也会醒，正好用来跑 demo_tick()
 */
static void demo_thread(void *arg)
{
    message msg;

    (void)arg;

    view_push();                                /* 先给渲染一份初值 */

    for(;;)
    {
        /* 100ms 一拍：有事件就处理，没事件也照样往下跑周期活 */
        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond(100)) == RT_EOK)
        {
            if(msg.type == 2)                   /* 2 = 管理端让退出 */
            {
                break;
            }

            if(msg.type == 0)                   /* 0 = 实体按键 */
            {
                demo_key((key_id_t)msg.code);
            }
            else                                /* 1 = 触摸控件 */
            {
                demo_set(msg.code, msg.value);
            }
        }

        demo_tick();
    }

    rt_sem_release(s_exit);                     /* 我走了，close 那边可以往下走 */
}

/**************************************************************************************************
 *                                        ⑥ 打开 / 关闭 / 投递 / 渲染
 **************************************************************************************************/
/**
 * @brief  打开：建信号量 + 队列 + 线程，任何一个失败就把已建的删掉
 * @note   重复调用直接返回（router 可能反复调）
 */
static void demo_open(void)
{
    if(s_tid != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_count  = 0;
    s_run    = 1;
    s_events = 0;

    s_exit = rt_sem_create("dexit", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("DEMO", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    s_tid  = rt_thread_create("DEMO", demo_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (s_tid == RT_NULL))
    {
        rt_kprintf("demo: open failed\r\n");

        if(s_tid  != RT_NULL) { rt_thread_delete(s_tid); s_tid  = RT_NULL; }
        if(s_q    != RT_NULL) { rt_mq_delete(s_q);       s_q    = RT_NULL; }
        if(s_exit != RT_NULL) { rt_sem_delete(s_exit);   s_exit = RT_NULL; }
        return;
    }

    rt_thread_startup(s_tid);
}

/**
 * @brief  关闭：让线程退出 -> 等它收尾 -> 删队列和信号量
 */
static void demo_close(void)
{
    message bye = {2, 0, 0};                    /* 2 = 该退出了 */
    message drop;
    uint8_t retry;

    if(s_tid == RT_NULL)
    {
        return;                                 /* 没开，什么都不用做 */
    }

    /* 队列可能刚好满：腾一个位置再投，最多试 20 次 */
    for(retry = 0; retry < 20; retry++)
    {
        if(rt_mq_send(s_q, &bye, sizeof(bye)) == RT_EOK)
        {
            break;
        }
        (void)rt_mq_recv(s_q, &drop, sizeof(drop), 0);
    }

    if(retry >= 20)
    {
        rt_kprintf("demo: quit msg failed\r\n");
        return;                                 /* 投不进去就别动，线程还在用这个队列 */
    }

    if(rt_sem_take(s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        rt_kprintf("demo: exit timeout\r\n");
        return;                                 /* 线程还没走，队列不能删 */
    }

    /* 线程已经自己退出（entry return），这里只回收资源 */
    rt_mq_delete(s_q);      s_q    = RT_NULL;
    rt_sem_delete(s_exit);  s_exit = RT_NULL;
    s_tid = RT_NULL;
}

/**
 * @brief  投递：把事件塞进自己的队列（跑在 kernel 线程，不阻塞）
 */
static void demo_post(const message *msg)
{
    if(s_q != RT_NULL)
    {
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
    }
}

/**
 * @brief  每帧渲染：只拿快照 + 调一个界面函数，不碰硬件
 */
static void demo_render(void)
{
    demo_view_t v;
    rt_base_t   level;

    level = rt_hw_interrupt_disable();
    v = s_view;
    rt_hw_interrupt_enable(level);

    demo_ui_render(v.count, v.run, v.events);
}

/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const task_t task_example =
{
    .name       = "DEMO",           /* 调试打印用，也是线程名（<= 7 字符） */
    .keep       = 0,                /* 0 = 切走就关线程 */

    .open       = demo_open,        /* 自己建资源 + 线程 */
    .close      = demo_close,       /* 自己让线程退出 + 删资源 */
    .post       = demo_post,        /* 自己收事件 */
    .render     = demo_render,      /* 前台每拍画界面 */
    .ui_destroy = demo_ui_destroy,  /* 关页时把页面还给 LVGL 堆 */
};
