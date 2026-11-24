/******************************************************************************
 * 文件名   : task_01.c
 * 说明     : 任务骨架 —— 由 tools/new_task.py 生成，把 task_01 换成真名
 *
 * 函数命名约定（都是 static，各文件互不影响）：
 *     task_open / task_close / task_post / task_render / task_ui_destroy
 *     task_thread / task_tick / task_key / task_set
 ******************************************************************************/

#include "router.h"
#include "key.h"
#include "task_01_ui.h"
#include "rtthread.h"

static rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */
static rt_mq_t     s_q    = RT_NULL;    /* 自己的队列 */
static void task_thread(void *arg);


//********************在这里写常用变量********************//




static void task_open(void)
{
    rt_thread_t tid;

    if(s_q != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_exit = rt_sem_create("sem", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("mq", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    tid    = rt_thread_create("thr", task_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (tid == RT_NULL))
    {
        rt_kprintf("task_01: open failed\r\n");

        if(tid    != RT_NULL) { rt_thread_delete(tid); }
        if(s_q    != RT_NULL) { rt_mq_delete(s_q);       s_q    = RT_NULL; }
        if(s_exit != RT_NULL) { rt_sem_delete(s_exit);   s_exit = RT_NULL; }
        return;
    }
//********************在这里写进入逻辑********************//




    rt_thread_startup(tid);
}


static void task_tick(void)
{
//********************在这里写循环逻辑********************//

}


static void task_key(key_id_t key)
{
//********************在这里写按键逻辑********************//
    switch(key)
    {
        case KEY_ID_1:
            break;

        case KEY_ID_2:
            break;

        case KEY_ID_3:
            break;

        case KEY_ID_4:
            break;

        default:                                /* 长按 / 双击暂不处理 */
            return;
    }
}


static void task_set(uint8_t id, int32_t value)
{
//********************在这里写控件逻辑********************//
    (void)value;

    switch(id)
    {
        case 1:                                 /* id = 1 的控件 */
            break;

        default:
            return;
    }
}


static void task_close(void)
{
    message bye = {2, 0, 0};                    /* 2 = QUIT，该退出了 */
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
        rt_kprintf("task_01: quit msg failed\r\n");
        return;                                 /* 投不进去就别动，线程还在用这个队列 */
    }

    if(rt_sem_take(s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        rt_kprintf("task_01: exit timeout\r\n");
        return;                                 /* 线程还没走，队列不能删 */
    }

    /* 线程已经自己退出（entry return），这里只回收资源 */
    rt_mq_delete(s_q);      s_q    = RT_NULL;
    rt_sem_delete(s_exit);  s_exit = RT_NULL;
}


static void task_render(void)
{
    /* 贴完设计器生成的 UI 后，把下面这行的注释去掉，按你的数据传参 */
    /* task_01_ui_render(); */
}


static void task_ui_destroy(void)
{
    /* 贴完设计器生成的 UI 后，把下面这行的注释去掉 */
    /* task_01_ui_destroy(); */
}


static void task_thread(void *arg)
{
    message msg;

    (void)arg;

    for(;;)
    {
        /* 10ms 一拍：有事件就处理，没事件也照样往下跑周期活 */
        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond(10)) == RT_EOK)
        {
            if(msg.type == QUIT)                /* 管理端让退出 */
            {
                break;
            }

            if(msg.type == KEY)                 /* 实体按键 */
            {
                task_key(msg.code);
            }
            else                                /* 触摸控件 */
            {
                task_set(msg.code, msg.value);
            }
        }

        task_tick();
    }

    rt_sem_release(s_exit);                     /* 我走了，close 那边可以往下走 */
}


static void task_post(const message *msg)
{
    if(s_q != RT_NULL)
    {
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
    }
}


/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const task_t task_01 =
{
    .name       = "01",           /* 线程名，<= 7 字符 */
    .keep       = 0,                /* 1 = 前台常驻 */

    .open       = task_open,        /* 进入 task */
    .close      = task_close,       /* 退出 task */
    .post       = task_post,        /* 事件收取 */
    .render     = task_render,      /* 接管屏幕刷新 */
    .ui_destroy = task_ui_destroy,  /* lvgl 卸载 */
};
