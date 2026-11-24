/******************************************************************************
 * 文件名   : key.c
 * 芯片     : CH32V307VCT6
 * 说明     : 4 个板载按键驱动（EXTI 中断 + 消抖线程）
 *
 * 数据流：
 *     EXTI 中断（双边沿） → 释放信号量 → key 线程 → 15ms 消抖 → 产生按下事件
 *
 * 为什么中断里只放一个信号量：
 *     中断里读 GPIO 容易把抖动采样进来，而且拖长中断时间。
 *     让线程在 15ms 后统一读一次稳定电平，中断本身只有几条指令。
 ******************************************************************************/

#include "key.h"
#include "rtthread.h"
#include "rtdevice.h"

/**************************************************************************************************
 *                                        配置
 **************************************************************************************************/
#define KEY_THREAD_STACK        1536
#define KEY_THREAD_PRIORITY     8           /* 比 UI 线程高，保证按键响应及时 */
#define KEY_THREAD_TICK         5
#define KEY_DEBOUNCE_MS         15          /* 消抖窗口 */
#define KEY_SYNC_MS             100         /* 没有中断时的兜底同步周期 */
#define KEY_LONG_MS             800         /* 按住超过这个时间算长按 */
#define KEY_DBL_MS              300         /* 两次按下间隔在此时间内算双击 */
#define KEY_SCAN_MS             10          /* 按住期间的轮询周期，长按靠它计时 */

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
static struct rt_semaphore s_key_sem;
static struct rt_thread    s_key_thread;
static rt_uint8_t          s_key_stack[KEY_THREAD_STACK];

static key_event_cb_t s_key_cb  = RT_NULL;
static uint8_t        s_stable  = 0;        /* 上一次确认过的稳定电平 */
static uint8_t        s_inited  = 0;

/* 按键编号 -> RT-Thread 引脚号 */
/* 每个物理键一份状态，长按 / 双击都靠它 */
typedef struct
{
    uint32_t down_tick;     /* 这次按下的时刻 */
    uint32_t up_tick;       /* 上次松开的时刻（0 = 还没松开过） */
    uint8_t  down;          /* 1 = 现在按着 */
    uint8_t  long_sent;     /* 这次按住已经报过长按 */
    uint8_t  dbl_sent;      /* 这次按住算双击的第二下，松开不再报短按 */
} key_state_t;

static key_state_t s_key[KEY_ID_MAX];

static const rt_base_t s_rt_pin[KEY_ID_MAX] =
{
    KEY_RT_PIN_1, KEY_RT_PIN_2, KEY_RT_PIN_3, KEY_RT_PIN_4
};

/**************************************************************************************************
 *                                        内部函数
 **************************************************************************************************/
/* EXTI 回调（中断上下文），只做一件事：唤醒按键线程 */
static void key_isr(void *args)
{
    (void)args;

    rt_sem_release(&s_key_sem);
}

/* drv_gpio 在开中断时会把引脚改成浮空输入，这里恢复成内部上拉，避免悬空误触发 */
static void key_restore_pullup(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    GPIO_InitStructure.GPIO_Pin   = KEY_PIN_ALL;
    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(KEY_GPIO, &GPIO_InitStructure);
}

/* 按键线程：被中断唤醒 -> 消抖 -> 和稳定值比较 -> 回调 */
/* 发一个按键事件（0~3 短按 / 5~8 长按 / 9~12 双击） */
static void key_emit(key_id_t id)
{
    if(s_key_cb != RT_NULL)
    {
        s_key_cb(id);
    }
}

/**
 * @brief  扫一遍 4 个键，跑状态机
 * @note   只比较"现在"和"上一拍"，不依赖 EXTI：按下/松开/长按都从这里出
 */
static void key_scan(void)
{
    uint8_t  now = key_state_get();
    uint32_t t   = rt_tick_get();
    uint32_t long_tick = rt_tick_from_millisecond(KEY_LONG_MS);
    uint32_t dbl_tick  = rt_tick_from_millisecond(KEY_DBL_MS);
    uint8_t  i;

    for(i = 0; i < KEY_ID_MAX; i++)
    {
        key_state_t *k  = &s_key[i];
        uint8_t      on = (uint8_t)(((now & KEY_BIT(i)) != 0) ? 1 : 0);

        if((on != 0) && (k->down == 0))                 /* 按下沿 */
        {
            k->down      = 1;
            k->down_tick = t;
            k->long_sent = 0;

            /* 上次松开离现在够近：这是双击的第二下，按下沿就报，不用等松开 */
            if((k->up_tick != 0) && ((t - k->up_tick) <= dbl_tick))
            {
                k->dbl_sent = 1;
                key_emit((key_id_t)(KEY_ID_DBL_1 + i));
            }
            else
            {
                k->dbl_sent = 0;
            }
        }
        else if((on == 0) && (k->down != 0))            /* 松开沿 */
        {
            k->down    = 0;
            k->up_tick = t;

            /* 没报过长按、也不是双击的第二下，才算一次短按 */
            if((k->long_sent == 0) && (k->dbl_sent == 0))
            {
                key_emit((key_id_t)i);
            }
        }
        else if((on != 0) && (k->down != 0) && (k->long_sent == 0))
        {
            if((t - k->down_tick) >= long_tick)         /* 按住够久：报长按 */
            {
                k->long_sent = 1;
                key_emit((key_id_t)(KEY_ID_LONG_1 + i));
            }
        }
    }

    s_stable = now;
}

/* 按键线程：中断唤醒（消抖）或定时轮询（长按计时） -> 扫一遍 */
static void key_entry(void *parameter)
{
    (void)parameter;

    for(;;)
    {
        /* 有键按着时 10ms 扫一次（长按要计时），全松开时 100ms 一次就够 */
        rt_int32_t wait = (s_stable != 0) ? (rt_int32_t)rt_tick_from_millisecond(KEY_SCAN_MS)
                                          : (rt_int32_t)rt_tick_from_millisecond(KEY_SYNC_MS);

        if(rt_sem_take(&s_key_sem, wait) == RT_EOK)
        {
            /* 有边沿：等电平稳下来再扫，顺便把期间攒的中断清掉 */
            rt_sem_control(&s_key_sem, RT_IPC_CMD_RESET, RT_NULL);
            rt_thread_mdelay(KEY_DEBOUNCE_MS);
            rt_sem_control(&s_key_sem, RT_IPC_CMD_RESET, RT_NULL);
        }

        key_scan();
    }
}

/**************************************************************************************************
 *                                        对外函数
 **************************************************************************************************/
/**
 * @brief  初始化按键
 */
int key_init(void)
{
    for(uint8_t i = 0; i < KEY_ID_MAX; i++)
    {
        rt_pin_mode(s_rt_pin[i], PIN_MODE_INPUT_PULLUP);
        rt_pin_attach_irq(s_rt_pin[i], PIN_IRQ_MODE_RISING_FALLING, key_isr, RT_NULL);
        rt_pin_irq_enable(s_rt_pin[i], PIN_IRQ_ENABLE);
    }

    /* 恢复内部上拉（drv_gpio 的双边沿模式会配置成浮空输入） */
    key_restore_pullup();

    rt_sem_init(&s_key_sem, "key", 0, RT_IPC_FLAG_FIFO);

    s_stable = key_state_get();                 /* 用当前电平做初值，避免上电误报 */

    if(rt_thread_init(&s_key_thread, "key", key_entry, RT_NULL,
                      s_key_stack, sizeof(s_key_stack),
                      KEY_THREAD_PRIORITY, KEY_THREAD_TICK) != RT_EOK)
    {
        return -1;
    }

    s_inited = 1;

    return 0;
}

/**
 * @brief  启动按键线程
 */
int key_start(void)
{
    if(!s_inited)
    {
        return -1;
    }

    return (rt_thread_startup(&s_key_thread) == RT_EOK) ? 0 : -1;
}

/**
 * @brief  注册按键回调
 */
void key_event_register(key_event_cb_t cb)
{
    s_key_cb = cb;
}

/**
 * @brief  读取当前按键电平
 */
uint8_t key_state_get(void)
{
    uint8_t keys = 0;

    if(KEY_PUSH_BUTTON_1()) { keys |= KEY_BUTTON_1; }
    if(KEY_PUSH_BUTTON_2()) { keys |= KEY_BUTTON_2; }
    if(KEY_PUSH_BUTTON_3()) { keys |= KEY_BUTTON_3; }
    if(KEY_PUSH_BUTTON_4()) { keys |= KEY_BUTTON_4; }

    return keys;
}

/**
 * @brief  某个按键是否按下（已消抖）
 */
uint8_t key_is_pressed(key_id_t id)
{
    if(id >= KEY_ID_MAX)
    {
        return 0;
    }

    return (s_stable & KEY_BIT(id)) ? 1 : 0;
}
