#include "kernel.h"
#include "rtthread.h"
#include "lvgl/lvgl.h"

static rt_mq_t message_q = RT_NULL;        //key线程事件队列
static evt_handler_t event    = RT_NULL;   //事件处理
static render_cb_t   render = RT_NULL;     //事件刷新

static void kernel_loop(void *parameter)
{
    message ev;

    (void)parameter;

    for(;;)
    {
        if(rt_mq_recv(message_q, &ev, sizeof(ev),
                      rt_tick_from_millisecond(10)) == RT_EOK)
        {
            if(event != RT_NULL)         //有没有事件传入
            {
                event(&ev);              /* 交给绑进来的处理函数 */
            }
        }
        if(render != RT_NULL)
        {
            render();
        }
        lv_timer_handler();
    }
}


void kernel_post(uint8_t type, uint8_t code, int32_t value)     //事件队列投递
{
    message ev = {type, code, value};

    if(message_q == RT_NULL)                    //队列还没建，直接丢
    {
        return;
    }

    rt_mq_send(message_q, &ev, sizeof(ev));
}


static struct rt_thread s_tid;                  //kernel线程
static rt_uint8_t s_stack[6144];                //kernel线程栈
void kernel_run(evt_handler_t on_evt, render_cb_t on_render)
{
    event    = on_evt;                       //事件入口
    render = on_render;                    //事件调用
    message_q = rt_mq_create("evt", sizeof(message), 8, RT_IPC_FLAG_FIFO);      //创建信息队列
    rt_thread_init(&s_tid, 
                    "kernel", 
                    kernel_loop, 
                    RT_NULL,
                    s_stack, 
                    sizeof(s_stack), 
                    9, 
                    10);                            //核心线程
    rt_thread_startup(&s_tid);
}

/**************************************************************************************************
 *                                        MSH 调试
 **************************************************************************************************/
static void kernel_cmd(void)
{
}
MSH_CMD_EXPORT_ALIAS(kernel_cmd, kernel, show kernel loop counter);
