/******************************************************************************
 * 文件名   : menu.c
 * 说明     : MENU —— 任务选择页（首页），一屏按钮，点哪个进哪个
 *
 * 交互：
 *     点按钮  按钮发 type=1 控件事件（编号 = 下标+1），收到就 router_go()
 *     K1/K2   移动高亮（只改边框颜色）
 *     K3      进当前高亮那个
 *     K4      router 自己拦下，回首页
 *
 * 一个任务的完整样子（本文件和 task_example.c 结构完全一致）：
 *     ① 任务表    g_app_list[]：按钮墙上摆哪些任务
 *     ② 资源      s_exit / s_q / s_tid：open 建，close 删
 *     ③ 逻辑      menu_key() / menu_set() / menu_enter()：跑在自己的线程里
 *     ④ 线程本体  menu_thread()：rt_thread_create 的入口就是它
 *     ⑤ 生命周期  open / close / post
 *     ⑥ 渲染      menu_render()：跑在 kernel 线程，只在这里碰 lv_
 *
 * 资源为什么用 create/delete 而不是静态 init：
 *     rt_sem_init / rt_mq_init 这类"静态初始化"会把对象插进内核对象链表，
 *     重复调就重复插，链表会自指 → 之后任何遍历都死循环（RT_DEBUG 关了不报错）。
 *     所以资源统一 open 建、close 删，进出多少次都只有一份。
 ******************************************************************************/

#include "router.h"
#include "kernel.h"
#include "key.h"
#include "ui/menu_ui.h"
#include "rtthread.h"

/**************************************************************************************************
 *                                        ① 任务表
 **************************************************************************************************/
/* 任务实例由各自的 task_x.c 定义，这里用到哪个就 extern 哪个 */
extern const task_t task_example;

/* 按钮墙上的顺序：2 列，从左到右、从上到下 */
static const task_t *const g_app_list[] =
{
    &task_example,
};

/* 一共几个任务：sizeof 算出来，加行不用改别的地方 */
static const uint8_t g_app_num = (uint8_t)(sizeof(g_app_list) / sizeof(g_app_list[0]));

/* 按钮显示的名字：从任务表抄一份，界面层不直接摸 task_t */
static const char *s_names[8];

/**************************************************************************************************
 *                                        ② 状态与资源
 **************************************************************************************************/
/* 当前高亮第几个，0 开始。用 static 是为了退回首页时还停在原来那一项 */
static uint8_t s_cursor = 0;

/* 自己的三件资源：信号量（退出握手）、队列、线程 —— open 建、close 删 */
static rt_sem_t    s_exit = RT_NULL;
static rt_mq_t     s_q    = RT_NULL;
static rt_thread_t s_tid  = RT_NULL;

/**************************************************************************************************
 *                                        ③ 逻辑（跑在自己的线程里）
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

/**
 * @brief  触摸控件事件
 * @param  id     按钮编号 = 下标 + 1（见 ui/menu_ui.c）
 * @param  value  按钮用不上
 * @note   和 task_example.c 的 demo_set() 一一对应：界面只发事件，这里解释编号
 */
static void menu_set(uint8_t id, int32_t value)
{
    (void)value;

    if(id > 0)
    {
        menu_enter((uint8_t)(id - 1));
    }
}

/**************************************************************************************************
 *                                        ④ 线程本体
 **************************************************************************************************/
/**
 * @brief  线程入口：rt_thread_create 直接用的就是这个函数
 * @param  arg  没用到
 * @note   菜单是静态页，没有周期活，所以死等事件（RT_WAITING_FOREVER），
 *         一点空转都没有。收到 type=2 就退出，最后 release 自己的退出信号量。
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
        else                                    /* 1 = 触摸控件 */
        {
            menu_set(msg.code, msg.value);
        }
    }

    rt_sem_release(s_exit);                     /* 我走了，close 那边可以往下走 */
}

/**************************************************************************************************
 *                                        ⑤ 打开 / 关闭 / 投递
 **************************************************************************************************/
/**
 * @brief  打开：建信号量 + 队列 + 线程，任何一个失败就把已建的删掉
 * @note   重复调用直接返回（router 可能反复调）
 */
static void menu_open(void)
{
    if(s_tid != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_exit = rt_sem_create("mexit", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("MENU", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    s_tid  = rt_thread_create("MENU", menu_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (s_tid == RT_NULL))
    {
        rt_kprintf("menu: open failed\r\n");

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
static void menu_close(void)
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
        rt_kprintf("menu: quit msg failed\r\n");
        return;                                 /* 投不进去就别动，线程还在用这个队列 */
    }

    if(rt_sem_take(s_exit, rt_tick_from_millisecond(3000)) != RT_EOK)
    {
        rt_kprintf("menu: exit timeout\r\n");
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
static void menu_post(const message *msg)
{
    if(s_q != RT_NULL)
    {
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
    }
}

/**************************************************************************************************
 *                                        ⑥ 渲染（kernel 线程）
 **************************************************************************************************/
/**
 * @brief  每帧渲染：搭按钮墙 + 更新高亮
 * @note   名字表在这里填：render 一定先于第一帧执行，而任务线程可能还没被调度到
 */
static void menu_render(void)
{
    uint8_t i;

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

    .open       = menu_open,        /* 自己建资源 + 线程 */
    .close      = menu_close,       /* 自己让线程退出 + 删资源 */
    .post       = menu_post,        /* 自己收事件 */
    .render     = menu_render,      /* 前台每拍画界面 */
    .ui_destroy = menu_ui_destroy,  /* 关页时把页面还给 LVGL 堆 */
};
