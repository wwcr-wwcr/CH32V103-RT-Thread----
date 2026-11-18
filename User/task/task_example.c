/******************************************************************************
 * 文件名   : task_example.c
 * 说明     : EXAMPLE —— 模板任务（纯软件演示，不碰任何硬件）
 *
 * 这个文件就是一个任务的完整样子，复制它改业务就能长出新页面：
 *
 *     ① 线程资源     s_q / s_tid / s_exit 三个静态变量，自己管
 *     ② 线程本体     demo_thread()：rt_thread_create 的入口就是它
 *     ③ 节奏         80 行那句 rt_mq_recv(..., 100ms)：多少毫秒一拍，这里说了算
 *     ④ 打开/关闭    demo_open() / demo_close()，幂等，router 反复调没事
 *     ⑤ 投递         demo_post()：router 把事件丢进我的队列
 *     ⑥ 渲染         demo_render()：只读快照，所有 lv_ 都从这进（kernel 线程）
 *
 * 演示内容：
 *     每 100ms 计一次数
 *     K1 暂停 / 继续      K2 +10      K3 清零
 *     触摸 +10 / CLR / QUIT（QUIT = router_go(RT_NULL) 回首页）
 *
 * 换成真实业务时，只改 demo_tick()（周期活）和 demo_key()/demo_set()（事件处理）。
 ******************************************************************************/

#include "router.h"
#include "ui/task_example_ui.h"
#include "rtthread.h"
#include "rthw.h"
#include "key.h"
/**************************************************************************************************
 *                                        数据（只有本线程写）
 **************************************************************************************************/
static uint32_t s_count  = 0;       /* 计数器 */
static uint8_t  s_run    = 1;       /* 1 = 在计数 */
static uint16_t s_events = 0;       /* 按了多少下（按键 + 触摸） */

/**************************************************************************************************
 *                                        快照（本线程写 / render 读）
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

static rt_mq_t     s_q   = RT_NULL;     /* 我自己的队列 */
static rt_thread_t s_tid = RT_NULL;     /* 我自己的线程 */
static struct rt_semaphore s_exit;      /* 退出握手：线程 exit，close 等它 */
static void demo_tick(void)
{
    if(s_run != 0)
    {
        s_count++;
        view_push();
    }
}
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

        default:
            return;
    }

    s_events++;
    view_push();
}
static void demo_set(uint8_t id, int32_t value)         //触摸
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
        case 4:                                 /* QUIT：回首页（等价于按 K4） */
            /* 注意：绝不能在任务自己的线程里调 demo_close()！
             * close 会等"本线程退出"，自己等自己必然死锁，超时后还会删掉
             * 自己正在用的队列。切页交给 router，它在 kernel 线程里关。 */
            router_go(RT_NULL);
            return;
    }

    s_events++;
    view_push();
}
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

    rt_sem_release(&s_exit);                    /* 我走了，close 那边可以往下走 */
}
static void demo_open(void)
{
    if(s_tid != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_count  = 0;
    s_run    = 1;
    s_events = 0;

    rt_sem_init(&s_exit, "dexit", 0, RT_IPC_FLAG_FIFO);

    s_q = rt_mq_create("DEMO", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    if(s_q == RT_NULL)
    {
        rt_kprintf("demo: mq create failed\r\n");
        return;
    }

    /* 1024 = 栈；12 = 优先级（key=8、kernel=9） */
    s_tid = rt_thread_create("DEMO", demo_thread, RT_NULL, 1024, 12, 10);
    if(s_tid == RT_NULL)
    {
        rt_kprintf("demo: thread create failed\r\n");
        rt_mq_delete(s_q);
        s_q = RT_NULL;
        return;
    }

    rt_thread_startup(s_tid);
}
static void demo_close(void)
{
    message bye  = {2, 0, 0};                   /* 2 = 该退出了 */
    message drop;                               /* 腾队列时丢掉的旧事件 */
    uint8_t retry;

    if(s_tid == RT_NULL)
    {
        return;
    }

    /* 队列可能刚好满：丢一条旧事件腾位置，再投"退出"（最多试 20 次，不阻塞） */
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
        return;                                 /* 投不进去就别动，宁可留着也不踩内存 */
    }

    if(rt_sem_take(&s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        /* 线程没退：队列还在被它用，绝对不能删 */
        rt_kprintf("demo: exit timeout\r\n");
        return;
    }

    rt_mq_delete(s_q);
    s_q   = RT_NULL;
    s_tid = RT_NULL;
}
static void demo_post(const message *msg)
{
    if(s_q == RT_NULL)
    {
        return;
    }

    (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
}
static void demo_render(void)
{
    demo_view_t v;
    rt_base_t   level;

    level = rt_hw_interrupt_disable();
    v = s_view;
    rt_hw_interrupt_enable(level);

    demo_ui_render(v.count, v.run, v.events);
}


const task_t task_example =
{
    .name       = "DEMO",           /* 调试打印用，也是线程名（<= 7 字符） */
    .keep       = 0,                /* 0 = 切走就关线程 */

    .open       = demo_open,        /* 自己建队列 + 线程 */
    .close      = demo_close,       /* 自己让线程退出 + 删队列 */
    .post       = demo_post,        /* 自己收事件 */
    .render     = demo_render,      /* 前台每拍画界面 */
    .ui_destroy = demo_ui_destroy,  /* 关页时把页面还给 LVGL 堆 */
};
