#include "router.h"
#include "key.h"
#include "menu_ui.h"
#include "rtthread.h"

static rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */
static rt_mq_t     s_q    = RT_NULL;    /* 自己的队列 */
static void menu_thread(void *arg);

//extern const task_t task_x;                                           <----------在这添加
static const task_t *const g_app_list[] =
{
//  &task_x,                                                             <----------在这添加
};
static const uint8_t g_app_num = (uint8_t)(sizeof(g_app_list) / sizeof(g_app_list[0]));     //自动计算所有task
static const char *s_names[8];                 

//********************在这里写内置变量********************//
static uint8_t s_cursor = 0;        //当前task

static void menu_enter(uint8_t index)
{
//********************在这里写进入task逻辑********************//
    s_cursor = index;
    router_go(g_app_list[index]);
}

static void menu_key(key_id_t key)
{
//********************在这里写按键逻辑********************//
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

        case KEY_ID_4:                   
            break;

        default:                  
            return;
    }
}

static void menu_set(uint8_t id, int32_t value)
{
//********************在这里写触摸控件逻辑********************//
    if(id > 0)                                  /* 编号 = 按钮下标 + 1 */
    {
        menu_enter((uint8_t)(id - 1));
    }
}

static void menu_open(void)
{
    rt_thread_t tid;

    if(s_q != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

    s_exit = rt_sem_create("mexit", 0, RT_IPC_FLAG_FIFO);
    s_q    = rt_mq_create("MENU", sizeof(message), 4, RT_IPC_FLAG_FIFO);
    tid    = rt_thread_create("MENU", menu_thread, RT_NULL, 1024, 12, 10);

    if((s_exit == RT_NULL) || (s_q == RT_NULL) || (tid == RT_NULL))
    {
        rt_kprintf("menu: open failed\r\n");

        if(tid    != RT_NULL) { rt_thread_delete(tid); }
        if(s_q    != RT_NULL) { rt_mq_delete(s_q);       s_q    = RT_NULL; }
        if(s_exit != RT_NULL) { rt_sem_delete(s_exit);   s_exit = RT_NULL; }
        return;
    }
//********************在这里写进入逻辑********************//

    rt_thread_startup(tid);
}

static void menu_close(void)
{
    message bye = {2, 0, 0};                    /* 2 = 该退出了 */
    message drop;
    uint8_t retry;

    if(s_q == RT_NULL)
    {
        return;                                 /* 没开，什么都不用做 */
    }
//********************在这里写关闭逻辑(menu的关闭逻辑几乎不会运行)********************//

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
}

static void menu_render(void)
{
    uint8_t i;

    /* 名字表只填一次；render 一定先于第一帧执行，而任务线程可能还没被调度到 */
    if(s_names[0] == RT_NULL)
    {
        for(i = 0; i < g_app_num; i++)
        {
            s_names[i] = g_app_list[i]->name;
        }
    }

    menu_ui_render(s_names, g_app_num, s_cursor);
}

static void menu_thread(void *arg)
{
    message msg;

    (void)arg;

    for(;;)
    {
        /* 菜单是静态页，没有周期活，所以死等事件；收到 type=2 就退出 */
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

static void menu_post(const message *msg)
{
    if(s_q != RT_NULL)
    {
        (void)rt_mq_send(s_q, (void *)msg, sizeof(*msg));   /* 队列只读，转掉 const */
    }
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
