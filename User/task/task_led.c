#include "router.h"
#include "key.h"
#include "ui/task_led_ui.h"
#include "rtthread.h"
#include "veml7700.h"
#include "led.h"
#include "ch422.h"
static rt_sem_t    s_exit = RT_NULL;    /* 退出握手：线程 exit，close 等它 */
static rt_mq_t     s_q    = RT_NULL;    /* 自己的队列 */
static void demo_thread(void *arg);



//********************在这里写常用变量********************//
static uint8_t a = RT_NULL;
static uint8_t b = RT_NULL;
static float c = RT_NULL;
static uint16_t d = RT_NULL;

static uint32_t color[] = {
    0xff0000,
    0x00ff00,
    0x0000ff,
    0xffff00,
    0xff00ff,
    0x00ffff,
    0xffffff
};

static void demo_open(void)
{
    rt_thread_t tid;

    if(s_q != RT_NULL)
    {
        return;                                 /* 已经在跑 */
    }

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
//********************在这里写进入逻辑********************//
    c = veml7700_lux(50);
    CH422_Show("%5.1f",c);



    rt_thread_startup(tid);
}

static uint8_t is_down = 0;
static uint8_t bright_static = 50;
static uint8_t long_done = 0;        /* 放在文件顶部：这次按住是否已触发长按 */
static void demo_tick(void)
{
//********************在这里写循环逻辑********************//
    c = veml7700_lux(50);
    CH422_Show("%5.1f",c);
    if(c<=30){
        is_down++;
        if((is_down == 100) && (long_done == 0)){
            long_done     = 1;
            bright_static = (bright_static == 100) ? 50 : 100;
            //bright_static = 50 ? 100:50;
            led_set(color[b],bright_static);
            return;
        }
        return;
    }
    if(is_down && (long_done == 0)){
        b = (b + 1) % 8;
        led_set(color[b],bright_static);
    }
    is_down = 0;
    long_done  = 0;
}


static void demo_key(key_id_t key)
{
//********************在这里写按键逻辑********************//
    switch(key)
    {
        case KEY_ID_1:                          /* K1 暂停 / 继续 */
    
            break;

        case KEY_ID_2:                          /* K2 +10 */
            break;

        case KEY_ID_3:                          /* K3 清零 */
            break;

        case KEY_ID_4:  
            break;

        default:                                /* 长按 / 双击暂不处理 */
            return;
    }
}


static void demo_set(uint8_t id, int32_t value)
{
//********************在这里写控件逻辑********************//
    (void)value;

    switch(id)
    {
        case 1:                 //id为1的控件按下
            break;
    }

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
    led_set(LED_COLOR_BLACK,0);
    CH422_Show("    ");




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
    task_led_ui_render(1,c,is_down);
}


static void demo_thread(void *arg)
{
    message msg;

    (void)arg;

    for(;;)
    {
        /* 100ms 一拍：有事件就处理，没事件也照样往下跑周期活 */
        if(rt_mq_recv(s_q, &msg, sizeof(msg), rt_tick_from_millisecond(10)) == RT_EOK)
        {
            if(msg.type == QUIT)               /* 管理端让退出 */
            {
                break;
            }

            if(msg.type == KEY)                 /* 实体按键 */
            {
                demo_key(msg.code);
            }
            else                                /* 触摸控件 */
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
const task_t task_led =
{
    .name       = "TASK1",           /* 线程名*/
    .keep       = 0,                /* 1 = 前台 */

    .open       = demo_open,        /* 进入task */
    .close      = demo_close,       /* 退出task */
    .post       = demo_post,        /* 事件收取 */
    .render     = demo_render,      /* 接管屏幕刷新 */
    .ui_destroy = task_led_ui_destroy,  /* lvgl卸载 */
};
