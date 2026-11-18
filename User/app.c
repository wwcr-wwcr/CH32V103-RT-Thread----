/******************************************************************************
 * 文件名   : app.c
 * 说明     : 应用管理器：任务线程的生、死，以及"现在该跑哪一个"
 *
 * 队列归属：一个任务一条队列
 *     队列跟着线程一起建、一起删（app_attach / app_stop）。
 *     所以任务切换时天然是一条空队列，收不到上一个任务没处理完的事件，
 *     也不需要"切页前把队列倒干净"这种补丁。
 *     想同时跑多个任务，只要把 s_host 从 1 个改成数组就行，结构不用动。
 *
 * 事件怎么走：
 *     kernel -> app_handle(evt)
 *                  ├ type=0 按键   -> 当前任务队列 on_key（K4 在这里拦下，回首页）
 *                  └ type=1 控件   -> 当前任务队列 on_set
 *     投递只是 rt_mq_send，不阻塞，所以放在 kernel 线程里也安全。
 *
 * 切页怎么走：
 *     app_request(app) 只写 s_want
 *     下一拍 app_render() -> app_sync() 真正切：停老线程 -> 起新线程 -> 画新页 -> 删老页
 ******************************************************************************/

#include "app.h"
#include "rtthread.h"

/**************************************************************************************************
 *                                        内部状态
 **************************************************************************************************/
/* 当前任务线程的上下文：任务本体 + 它自己的队列 */
typedef struct
{
    const app_task_t *task;
    rt_mq_t           q;
} host_t;

/* 首页：上电先进它；任务页里按 K4 也是退回它 */
static const app_task_t *s_home = RT_NULL;

/* 当前应用 */
static const app_task_t *s_app = RT_NULL;

/* 想切到哪个应用。任何线程都能写，kernel 线程每拍读一次，所以要 volatile */
static const app_task_t *volatile s_want = RT_NULL;

/* 同一时刻只有一个任务线程，所以只有一份上下文 */
static host_t s_host;

/* 当前应用的线程句柄 */
static rt_thread_t s_tid = RT_NULL;

/* 任务线程收尾时 release 一下，切页的人靠它知道"线程真的没了" */
static struct rt_semaphore s_exit_sem;

/* 队列满丢掉的事件数，app 命令里能看 */
static uint32_t s_lost = 0;

/**************************************************************************************************
 *                                        投递
 **************************************************************************************************/
/**
 * @brief  把事件投给当前任务自己的队列
 * @note   type=2 是内部约定：这封消息的意思是"退出线程"
 */
static void deliver(uint8_t type, uint8_t code, int32_t value)
{
    evt_t ev;

    if(s_host.q == RT_NULL)
    {
        return;
    }

    ev.type  = type;
    ev.code  = code;
    ev.value = value;

    if(rt_mq_send(s_host.q, &ev, sizeof(ev)) != RT_EOK)
    {
        s_lost++;
    }
}

/**************************************************************************************************
 *                                        线程管理
 **************************************************************************************************/
/**
 * @brief  任务线程本体：所有任务都跑这一个函数，区别只在 parameter 指的上下文
 */
static void task_entry(void *parameter)
{
    const host_t *host = (const host_t *)parameter;
    const app_task_t *task = host->task;
    evt_t ev;
    rt_int32_t timeout;

    if(task->setup != RT_NULL)
    {
        task->setup();
    }

    for(;;)
    {
        /* 有 update 的任务 10ms 一醒；纯静态界面死等事件，一点 CPU 都不烧 */
        if(task->update != RT_NULL)
        {
            timeout = (rt_int32_t)rt_tick_from_millisecond(10);
        }
        else
        {
            timeout = (rt_int32_t)RT_WAITING_FOREVER;
        }

        if(rt_mq_recv(host->q, &ev, sizeof(ev), timeout) == RT_EOK)
        {
            if(ev.type == 2)                    /* 2 = 退出线程，走收尾流程 */
            {
                break;
            }

            if(ev.type == 1)                    /* 1 = 控件事件（滑条、按钮） */
            {
                if(task->on_set != RT_NULL)
                {
                    task->on_set(ev.code, ev.value);
                }
            }
            else if(task->on_key != RT_NULL)    /* 0 = 按键 */
            {
                task->on_key((key_id_t)ev.code);
            }
        }

        if(task->update != RT_NULL)
        {
            task->update();
        }
    }

    if(task->teardown != RT_NULL)
    {
        task->teardown();
    }

    /* 通知 kernel 线程"我走完了"，然后函数返回，内核自己回收这个线程。
     * 别在这里调 rt_thread_delete(self)：返回后内核还会再走一遍退出流程。 */
    rt_sem_release(&s_exit_sem);
}

/**
 * @brief  建任务线程 + 给它一条自己的队列（时间片统一 10，优先级用任务自己的）
 */
static void app_attach(const app_task_t *app)
{
    s_host.task = app;
    s_host.q    = rt_mq_create(app->name, sizeof(evt_t), 4, RT_IPC_FLAG_FIFO);
    if(s_host.q == RT_NULL)
    {
        rt_kprintf("app: mq create failed\r\n");
        return;
    }

    s_tid = rt_thread_create(app->name, task_entry, &s_host,
                             app->stack_size, app->priority, 10);
    if(s_tid == RT_NULL)
    {
        rt_kprintf("app: create thread %s failed\r\n", app->name);
        rt_mq_delete(s_host.q);
        s_host.q    = RT_NULL;
        s_host.task = RT_NULL;
        return;
    }

    rt_thread_startup(s_tid);
}

/**
 * @brief  停任务线程，并把它的队列一起删掉（会阻塞到线程真的退出）
 */
static void app_stop(void)
{
    if(s_tid == RT_NULL)
    {
        return;
    }

    /* 上一次超时留下的信号量先清掉，不然这次 take 会立刻返回 */
    (void)rt_sem_control(&s_exit_sem, RT_IPC_CMD_RESET, RT_NULL);

    deliver(2, 0, 0);                           /* 2 = 退出线程 */

    /* 最多等 3 秒。真超时了也不强杀：任务的 teardown 没跑完就 free 栈，
     * 后面踩的都是坏内存，宁可留一个僵尸线程换这条打印出来。 */
    if(rt_sem_take(&s_exit_sem, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        rt_kprintf("app: task exit timeout\r\n");
    }

    s_tid = RT_NULL;

    /* 线程没了，队列跟着删：下一个任务用它自己的新队列，天然是空的 */
    if(s_host.q != RT_NULL)
    {
        rt_mq_delete(s_host.q);
        s_host.q = RT_NULL;
    }

    s_host.task = RT_NULL;
}

/**
 * @brief  切页：停老的、起新的、画新的、删老的
 * @note   顺序不能换：LVGL 不许删正在显示的 screen，
 *         必须等新页面 load 上去，老页面才不在前台。
 */
static void app_switch(const app_task_t *want)
{
    const app_task_t *old = s_app;

    if(want == old)
    {
        return;
    }

    /* 1. 老应用退场：删线程和它的队列（页面先留着，它还是当前屏） */
    if(old != RT_NULL)
    {
        app_stop();
    }

    /* 2. 新应用上场，并且立刻画一帧 —— 这帧会把新页面 load 成当前屏 */
    s_app = want;
    if(want != RT_NULL)
    {
        app_attach(want);

        if(want->render != RT_NULL)
        {
            want->render();
        }
    }

    /* 3. 老页面这会儿才不在前台，可以安全删掉，把 LVGL 堆还回去 */
    if((old != RT_NULL) && (old->ui_destroy != RT_NULL))
    {
        old->ui_destroy();
    }
}

/**
 * @brief  有切页请求就切（只 write 过 s_want，真正干活在这）
 */
static void app_sync(void)
{
    if(s_want != s_app)
    {
        app_switch(s_want);
    }
}

/**************************************************************************************************
 *                                        对外接口
 **************************************************************************************************/
void app_init(const app_task_t *home)
{
    s_home = home;
    s_want = home;
    s_app  = RT_NULL;

    /* 退出握手：初值 0，任务线程 release 一次，切页的人 take 一次 */
    rt_sem_init(&s_exit_sem, "exit", 0, RT_IPC_FLAG_FIFO);
}

void app_handle(const evt_t *ev)
{
    if(ev == RT_NULL)
    {
        return;
    }

    if(ev->type == 0)                           /* 0 = 按键 */
    {
        /* K4 = 全局退出键，只在任务页有效，永远不下发给任务 */
        if(ev->code == KEY_ID_4)
        {
            if(s_app != s_home)
            {
                s_want = s_home;                /* 回首页 */
            }
            return;
        }

        if((s_app != RT_NULL) && (s_app->on_key != RT_NULL))
        {
            deliver(0, ev->code, 0);
        }
    }
    else if(ev->type == 1)                      /* 1 = 控件事件 */
    {
        /* 首页不发控件事件：菜单页的触摸是合成为按键的，不走这条 */
        if((s_app != RT_NULL) && (s_app != s_home) && (s_tid != RT_NULL))
        {
            deliver(1, ev->code, ev->value);
        }
    }
}

void app_render(void)
{
    app_sync();                                 /* 先把切页请求处理掉 */

    if((s_app != RT_NULL) && (s_app->render != RT_NULL))
    {
        s_app->render();
    }
}

void app_request(const app_task_t *app)
{
    /* 传 NULL = 回首页：任务里的"退出"按钮直接 app_request(RT_NULL) */
    s_want = (app != RT_NULL) ? app : s_home;
}

/**************************************************************************************************
 *                                        MSH 调试
 **************************************************************************************************/
static void app_cmd(void)
{
    rt_size_t total = 0;
    rt_size_t used  = 0;

    rt_memory_info(&total, &used, RT_NULL);

    rt_kprintf("app  : cur=%s home=%s q=%s lost=%lu\r\n",
               (s_app != RT_NULL) ? s_app->name : "-",
               (s_home != RT_NULL) ? s_home->name : "-",
               (s_host.q != RT_NULL) ? "on" : "off",
               (unsigned long)s_lost);
    rt_kprintf("heap : used=%lu free=%lu\r\n",
               (unsigned long)used, (unsigned long)(total - used));
}
MSH_CMD_EXPORT_ALIAS(app_cmd, app, show current app and heap usage);
