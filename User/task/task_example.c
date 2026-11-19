#include "router.h"
#include "key.h"
#include "ui/task_example_ui.h"
#include "rtthread.h"

static rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */
static rt_mq_t     s_q    = RT_NULL;    /* 自己的队列 */
static void demo_thread(void *arg);



//********************在这里写常用变量********************//
static volatile uint32_t s_count  = 0;      /* 计数器 */
static volatile uint8_t  s_run    = 1;      /* 1 = 在计数 */
static volatile uint16_t s_events = 0;      /* 按了多少下（按键 + 触摸） */




static void demo_tick(void)
{
//********************在这里写循环逻辑********************//
    if(s_run != 0)
    {
        s_count++;
    }

}


static void demo_key(key_id_t key)
{
//********************在这里写按键逻辑********************//
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

}


static void demo_set(uint8_t id, int32_t value)
{
//********************在这里写控件逻辑********************//
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
            router_go(RT_NULL);                 /* 回首页 */
            return;

        case 4:
            return;
        default:
            return;
    }

    s_events++;
}


static void demo_open(void)
{
    rt_thread_t tid;

    if(s_q != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_count  = 0;
    s_run    = 1;
    s_events = 0;

    s_exit = rt_sem_create("dexit", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("DEMO", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    tid    = rt_thread_create("DEMO", demo_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (tid == RT_NULL))
    {
        rt_kprintf("demo: open failed\r\n");

        if(tid    != RT_NULL) { rt_thread_delete(tid); }
        if(s_q    != RT_NULL) { rt_mq_delete(s_q);       s_q    = RT_NULL; }
        if(s_exit != RT_NULL) { rt_sem_delete(s_exit);   s_exit = RT_NULL; }
        return;
    }




    rt_thread_startup(tid);
}


static void demo_close(void)
{
    message bye = {2, 0, 0};             /* MSG_QUIT = 该退出了 */
    message drop;
    uint8_t retry;

    if(s_q == RT_NULL)
    {
        return;                                 /* 没开，什么都不用做 */
    }
//********************在这里写退出逻辑********************//




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
}


static void demo_render(void)
{
    demo_ui_render(s_count, s_run, s_events);
}


static void demo_thread(void *arg)
{
    message msg;

    (void)arg;

    for(;;)
    {
        /* 100ms 一拍：有事件就处理，没事件也照样往下跑周期活 */
        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond(100)) == RT_EOK)
        {
            if(msg.type == 2)            /* 管理端让退出 */
            {
                break;
            }

            if(msg.type == 0)             /* 实体按键 */
            {
                demo_key((key_id_t)msg.code);
            }
            else                                /* MSG_CTRL = 触摸控件 */
            {
                demo_set(msg.code, msg.value);
            }
        }

        demo_tick();
    }

    rt_sem_release(s_exit);                     /* 我走了，close 那边可以往下走 */
}


static void demo_post(const message *msg)
{
    if(s_q != RT_NULL)
    {
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
    }
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
