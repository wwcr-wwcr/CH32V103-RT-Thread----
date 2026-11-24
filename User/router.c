#include "router.h"
#include "key.h"
#include "rtthread.h"
static const task_t *s_home = RT_NULL;      //返回标准页
static const task_t *s_cur = RT_NULL;       //前台事件
static const task_t *volatile s_want = RT_NULL;     //将要切的事件

static void fore_switch(const task_t *want)         //切换事件调用
{
    const task_t *old = s_cur;

    if(want == old)
    {
        return;
    }                       //防止新任务是老任务

    /* 1. 新任务上场：open 由任务自己实现（重复调用要能挡住） */
    if(want != RT_NULL)
    {
        if(want->open != RT_NULL)
        {
            want->open();
        }

        s_cur = want;

        /* 2. 立刻画一帧：把新页面 load 成当前屏 */
        if(want->render != RT_NULL)
        {
            want->render();
        }
    }
    else
    {
        s_cur = RT_NULL;
    }

    /* 3. 老任务退场：常驻的留着继续跑，其它的关掉；页面一律还回去 */
    if((old != RT_NULL) && (old != s_cur))
    {
        if((old->keep == 0) && (old->close != RT_NULL))
        {
            old->close();
        }

        if(old->ui_destroy != RT_NULL)
        {
            old->ui_destroy();
        }
    }
}

void router_init(const task_t *home)
{
    s_home = home;
    s_want = home;
    s_cur  = RT_NULL;
}

//唯一经常使用的函数
void router_go(const task_t *task)
{
    s_want = (task != RT_NULL) ? task : s_home;     //在下一次render中刷新
}

void router_dispatch(const message *msg)
{
    if(msg == RT_NULL)
    {
        return;
    }

    /* K4 长按 = 全局退出键：任何页面都能回首页，永远不下发给任务 */
    if((msg->type == KEY) && (msg->code == KEY_ID_LONG_4))
    {
        if(s_cur != s_home)
        {
            s_want = s_home;
        }
        return;
    }

    if((s_cur != RT_NULL) && (s_cur->post != RT_NULL))
    {
        s_cur->post(msg);
    }
}

void router_render(void)
{
    if(s_want != s_cur)                         /* 有切页请求就切 */
    {
        fore_switch(s_want);                    /* 里面已经把新页画成当前屏了 */
        return;                                 /* 这一拍不重复画 */
    }

    if((s_cur != RT_NULL) && (s_cur->render != RT_NULL))
    {
        s_cur->render();
    }
}

/**************************************************************************************************
 *                                        MSH 调试
 **************************************************************************************************/
static void router_cmd(void)
{
    rt_kprintf("router: cur=%s home=%s want=%s\r\n",
               (s_cur  != RT_NULL) ? s_cur->name  : "-",
               (s_home != RT_NULL) ? s_home->name : "-",
               (s_want != RT_NULL) ? s_want->name : "-");
}
MSH_CMD_EXPORT_ALIAS(router_cmd, router, show current page and pending switch);
