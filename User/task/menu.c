/******************************************************************************
 * 文件名   : menu.c
 * 说明     : MENU —— 任务选择页（首页）
 *
 * 一屏摆完所有任务，每个任务一个按钮：
 *     点按钮  按钮发 type=1 控件事件，编号 = 下标+1，收到就 router_go()
 *     K1/K2   移动高亮（只改边框颜色）
 *     K3      进当前高亮那个
 *     K4      router 自己拦下，回首页
 *
 * 完整自治的一个任务，不依赖任何"任务框架"：
 *     线程     rt_thread_create 自己建，入口就是本文件的 menu_thread()
 *     队列     s_q 自己建、自己删
 *     退出握手 s_exit 自己初始化、自己 release
 *     router   只在切页时调 open() / close() / post() / render() 这 4 个回调
 *
 * 想加界面：写一个 task_xxx.c（同样自治）-> 下面 extern 一行 + g_app_list[] 加一行。
 ******************************************************************************/

#include "router.h"
#include "kernel.h"
#include "key.h"
#include "ui/menu_ui.h"
#include "rtthread.h"

/**************************************************************************************************
 *                                        任务表
 **************************************************************************************************/
/* 任务实例由各自的 task_x.c 定义，这里用到哪个就 extern 哪个 */
extern const task_t task_example;

/* 菜单里能进的任务：顺序 = 按钮墙上的顺序（2 列，从左到右、从上到下） */
static const task_t *const g_app_list[] =
{
    &task_example,
    &task_example,
};

/* 一共几个任务：sizeof 算出来，加行不用改别的地方 */
static const uint8_t g_app_num = (uint8_t)(sizeof(g_app_list) / sizeof(g_app_list[0]));

/* 按钮上显示的名字：从任务表里抄一份，菜单界面不直接摸 task_t */
static const char *s_names[8];

/**************************************************************************************************
 *                                        数据
 **************************************************************************************************/
/* 当前高亮第几个，0 开始。用 static 是为了退回首页时还停在原来那一项 */
static uint8_t s_cursor = 0;

/* 我自己的线程资源：队列、线程、退出握手信号量 */
static rt_mq_t     s_q   = RT_NULL;
static rt_thread_t s_tid = RT_NULL;
static struct rt_semaphore s_exit;

/**************************************************************************************************
 *                                        按键逻辑（跑在自己的线程里）
 **************************************************************************************************/
/**
 * @brief  进第 index 个任务
 */
static void menu_enter(uint8_t index)
{
    if(index >= g_app_num)
    {
        return;
    }

    s_cursor = index;
    router_go(g_app_list[index]);
}

/**
 * @brief  实体按键：K1/K2 移高亮，K3 进
 * @note   K4 被 router 拦下了，这里收不到
 */
static void menu_key(key_id_t key)
{
    switch(key)
    {
        case KEY_ID_1:                          /* K1 上一个，到头回绕 */
            s_cursor = (uint8_t)((s_cursor + g_app_num - 1) % g_app_num);
            break;

        case KEY_ID_2:                          /* K2 下一个 */
            s_cursor = (uint8_t)((s_cursor + 1) % g_app_num);
            break;

        case KEY_ID_3:                          /* K3 进当前高亮 */
            menu_enter(s_cursor);
            break;

        default:
            return;
    }
}

/**************************************************************************************************
 *                                        线程本体
 **************************************************************************************************/
/**
 * @brief  线程入口：这个是 rt_thread_create 的入口函数，不是被谁回调的
 * @param  arg  没用到
 * @note   收到 type=2 就退出循环，然后 release 自己的退出信号量。
 *         菜单是静态页，没有周期活，所以 recv 一直等，一点空转都没有。
 */
static void menu_thread(void *arg)
{
    message msg;

    (void)arg;

    for(;;)
    {
        if(rt_mq_recv(s_q, &msg, sizeof(msg), RT_WAITING_FOREVER) != RT_EOK)
        {
            break;
        }

        if(msg.type == 2)                       /* 2 = 管理端让退出 */
        {
            break;
        }

        if(msg.type == 0)                       /* 0 = 实体按键 */
        {
            menu_key((key_id_t)msg.code);
        }
        else if(msg.type == 1)                  /* 1 = 触摸按钮：编号 = 下标+1 */
        {
            if(msg.code > 0)
            {
                menu_enter((uint8_t)(msg.code - 1));
            }
        }
    }

    rt_sem_release(&s_exit);                    /* 我走了，close 那边可以往下走 */
}

/**************************************************************************************************
 *                                        打开 / 关闭 / 投递
 **************************************************************************************************/
/**
 * @brief  打开：建队列 + 建线程（重复调用直接返回）
 */
static void menu_open(void)
{
    if(s_tid != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    rt_sem_init(&s_exit, "mexit", 0, RT_IPC_FLAG_FIFO);

    s_q = rt_mq_create("MENU", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    if(s_q == RT_NULL)
    {
        rt_kprintf("menu: mq create failed\r\n");
        return;
    }

    /* 1024 = 栈；12 = 优先级（key=8、kernel=9） */
    s_tid = rt_thread_create("MENU", menu_thread, RT_NULL, 1024, 12, 10);
    if(s_tid == RT_NULL)
    {
        rt_kprintf("menu: thread create failed\r\n");
        rt_mq_delete(s_q);
        s_q = RT_NULL;
        return;
    }

    rt_thread_startup(s_tid);
}

/**
 * @brief  关闭：让线程退出 -> 等它收尾 -> 删队列
 */
static void menu_close(void)
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
        rt_kprintf("menu: quit msg failed\r\n");
        return;
    }

    if(rt_sem_take(&s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        /* 线程没退：队列还在被它用，绝对不能删 */
        rt_kprintf("menu: exit timeout\r\n");
        return;
    }

    rt_mq_delete(s_q);
    s_q   = RT_NULL;
    s_tid = RT_NULL;
}

/**
 * @brief  投递：把事件塞进自己的队列（跑在 kernel 线程，不阻塞）
 */
static void menu_post(const message *msg)
{
    if(s_q == RT_NULL)
    {
        return;
    }

    (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
}

/**************************************************************************************************
 *                                        渲染（kernel 线程）
 **************************************************************************************************/
/**
 * @brief  每帧渲染：搭按钮墙 + 更新高亮
 * @note   高亮没动时 menu_ui_render 内部直接返回，不会白重绘
 */
static void menu_render(void)
{
    uint8_t i;

    /* 名字抄一份给界面层：放在这里是因为 render 一定先于第一帧执行，
     * 而任务线程可能还没被调度到 */
    if(s_names[0] == RT_NULL)
    {
        for(i = 0; i < g_app_num; i++)
        {
            s_names[i] = g_app_list[i]->name;
        }
    }

    menu_ui_render(s_names, g_app_num, s_cursor);
}

/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const task_t task_menu =
{
    .name       = "MENU",           /* 调试打印用，也是线程名（<= 7 字符） */
    .keep       = 0,                /* 0 = 切走就关线程 */

    .open       = menu_open,        /* 自己建队列 + 线程 */
    .close      = menu_close,       /* 自己让线程退出 + 删队列 */
    .post       = menu_post,        /* 自己收事件 */
    .render     = menu_render,      /* 前台每拍画界面 */
    .ui_destroy = menu_ui_destroy,  /* 关页时把页面还给 LVGL 堆 */
};
