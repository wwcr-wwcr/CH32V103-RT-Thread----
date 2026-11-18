#include "app.h"
#include "ui/veml_1_ui.h"
#include "rtthread.h"
#include "rthw.h"
#include "key.h"
#include <ch422.h>
#include <led.h>
#include <veml7700.h>
/**************************************************************************************************
 *                                        业务参数
 **************************************************************************************************/
#define LUX_SAMPLE_DIV      5      /* update 10ms 一次，50 次 = 500ms 采一次 */
#define LUX_DARK            10      /* lux <= 10：全黑，LED 给 100% */
#define LUX_BRIGHT          300     /* lux >= 300：够亮，LED 给 0% */
#define EX_STEP             10

/**************************************************************************************************
 *                                        业务状态（只有逻辑线程会改）
 **************************************************************************************************/
static uint16_t s_lux    = 0;       /* 最新光强 */
static uint8_t  s_bright = 0;       /* 算出来的 LED 亮度 0~100 */
static uint8_t  s_led_on = 1;       /* 用户开关 */
static uint8_t  s_err    = 0;       /* 传感器初始化失败标志 */
static uint16_t s_div    = 0;       /* 分频计数 */

/**************************************************************************************************
 *                                        快照（逻辑线程写 / UI 线程读）
 **************************************************************************************************/
typedef struct
{
    uint16_t lux;
    uint8_t  bright;
    uint8_t  led_on;
} lux_view_t;

static lux_view_t s_view;

static void view_update(void)
{
    lux_view_t v;
    rt_base_t level;

    v.lux = s_lux;
    v.bright = s_bright;
    v.led_on  = s_led_on;

    level = rt_hw_interrupt_disable();
    s_view = v;
    rt_hw_interrupt_enable(level);
}

/* 环境越暗，LED 越亮 */
static uint8_t lux_to_bright(uint16_t lux)
{
    if(lux <= LUX_DARK)   { return 100; }
    if(lux >= LUX_BRIGHT) { return 0; }

    return (uint8_t)(100u - ((uint32_t)(lux - LUX_DARK) * 100u) / (LUX_BRIGHT - LUX_DARK));
}

static void led_apply(void)
{
    led_brightness_set(LED_ALL, (s_led_on != 0) ? s_bright : 0);
}
/**************************************************************************************************
 *                                        任务回调
 **************************************************************************************************/
/**
 * @brief  逻辑线程：进页面时调用一次
 * @note   在这里初始化硬件。耗时几十 ms 也没关系，
 *         但不要做无限等待（会影响退出页面的响应）。
 */
static void lux_setup(void)
{
    s_lux    = 0;
    s_bright = 0;
    s_led_on = 1;
    s_div    = 0;
    s_err    = 0;

    /* 注意：这里不能调 sensor_init()！
     * 它会 cs100_init()，而 CS100 和 LED PWM 抢同一个 TIM10。 */
    if(veml7700_init() != 0)
    {
        s_err = 1;
        rt_kprintf("lux: veml7700 init failed\r\n");
    }

    if(led_pwm_init() != 0)
    {
        rt_kprintf("lux: led pwm init failed\r\n");
    }

    if(ch422_init() != 0)
    {
        rt_kprintf("lux: ch422 init failed\r\n");
    }

    led_brightness_set(LED_ALL, 0);
    ch422_clear();

    view_update();
}

/**
 * @brief  逻辑线程：每 10ms 调用一次（前提是 task 描述里 update 不为 NULL）
 * @note   用分频做慢速任务，别真的每 10ms 干重活
 */
static void lux_update(void)
{
    if(++s_div < LUX_SAMPLE_DIV)
    {
        return;
    }
    s_div = 0;

    if(s_err == 0)
    {
        s_lux = veml7700_lux_get();     /* 芯片转换 400ms，500ms 采一次刚好 */
    }                      
    s_bright = lux_to_bright(s_lux);
    led_apply();

    /* 数码管 4 位，超过 9999 显示 9999 */
    ch422_show_number((s_lux > 9999u) ? 9999u : s_lux);
    view_update();
}

/**
 * @brief  逻辑线程：实体按键
 * @param  key  只会是 KEY_ID_1 / KEY_ID_2 / KEY_ID_3
 * @note   K4 是全局退出键，menu 永远不转发
 */
static void lux_on_key(key_id_t key)
{
    if(key == KEY_ID_1)
    {
        s_led_on = (uint8_t)((s_led_on == 0) ? 1 : 0);
        led_apply();
        view_update();
    }

    view_update();
}

/**
 * @brief  逻辑线程：触摸控件事件（滑条 / 按钮）
 * @param  id     控件编号，见 ui/task_example_ui.h
 * @param  value  控件给的数值（滑条位置、按钮自带的步进量）
 */
static void lux_on_set(uint8_t id, int32_t value)
{
    (void)value;

    if(id == 1){
        s_led_on = (uint8_t)((s_led_on == 0) ? 1 : 0);
        led_apply();
        view_update();
    }
    else if(id == 2){
        app_request(RT_NULL);   /* 传 NULL = 回首页（等价于按 K4） */
    }
}

/**
 * @brief  UI 线程：每 10ms 调用一次
 * @note   只做两件事：拿快照、调一个 xxx_ui_render
 */
static void lux_render(void)
{
    lux_view_t v;
    rt_base_t level;

    level = rt_hw_interrupt_disable();
    v = s_view;
    rt_hw_interrupt_enable(level);

    custom_ui_render(v.lux, v.bright, v.led_on);
}

/**
 * @brief  逻辑线程：退出页面时调用（K4 或切到别的任务）
 * @note   在这里把硬件恢复安全状态，否则会一直占用外设
 */
static void lux_teardown(void)
{
    led_brightness_set(LED_ALL, 0);     /* 关灯，别让 LED 一直亮着 */
    ch422_clear();                      /* 数码管清空 */
}

/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const app_task_t task_lux =
{
    .name       = "LUX",            /* 菜单显示名，<= 7 字符 */
    .stack_size = 1024,
    .priority   = 12,
    .setup      = lux_setup,
    .update     = lux_update,
    .on_key     = lux_on_key,
    .on_set     = lux_on_set,
    .render     = lux_render,
    .teardown   = lux_teardown,
    .ui_destroy = custom_ui_destroy,
};