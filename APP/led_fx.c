/******************************************************************************
 * 文件名   : led_fx.c
 * 说明     : RGB 灯效实现 —— 动态模式由内部 rt_timer 驱动（10ms 一步）
 *
 * 内存：调色板 40 字节（在 led_fx.h 里定义一次），没有别的数组。
 ******************************************************************************/

#define LED_FX_PALETTE_DEFINE           /* 让头文件把调色板实例化出来 */
#include "led_fx.h"
#include "led.h"
#include <rtthread.h>

/**************************************************************************************************
 *                                        状态
 **************************************************************************************************/
typedef enum
{
    M_STEADY = 0,
    M_BREATH,
    M_BLINK,
    M_RAINBOW
} fx_mode_t;

static uint32_t   s_rgb     = 0xff0000;     /* 当前颜色（0xRRGGBB） */
static fx_mode_t  s_mode    = M_STEADY;
static uint16_t   s_period  = 2000;         /* 动态模式的周期（ms） */
static uint8_t    s_off     = 0;            /* 1 = 被 off() 关掉了 */
static rt_tick_t  s_phase_t0;               /* 相位基准 */
static rt_timer_t s_timer   = RT_NULL;      /* 10ms 周期定时器 */

/* 渐变 */
static uint32_t   s_fade_from;
static uint32_t   s_fade_to;
static uint16_t   s_fade_ms;
static rt_tick_t  s_fade_t0;
static uint8_t    s_fading;

/**************************************************************************************************
 *                                        内部工具
 **************************************************************************************************/
/* 0..1000 三通道线性插值 */
static uint32_t lerp_rgb(uint32_t a, uint32_t b, uint32_t p)
{
    int32_t ar = (int32_t)((a >> 16) & 0xFFU);
    int32_t ag = (int32_t)((a >> 8) & 0xFFU);
    int32_t ab = (int32_t)(a & 0xFFU);
    int32_t br = (int32_t)((b >> 16) & 0xFFU);
    int32_t bg = (int32_t)((b >> 8) & 0xFFU);
    int32_t bb = (int32_t)(b & 0xFFU);
    uint32_t r;
    uint32_t g;
    uint32_t bl;

    r  = (uint32_t)(ar + (br - ar) * (int32_t)p / 1000);
    g  = (uint32_t)(ag + (bg - ag) * (int32_t)p / 1000);
    bl = (uint32_t)(ab + (bb - ab) * (int32_t)p / 1000);

    return (r << 16) | (g << 8) | bl;
}

/* HSV -> RGB：h 0..359，s/v 0..100（整数实现，不用浮点） */
static uint32_t hsv2rgb(uint16_t h, uint8_t s, uint8_t v)
{
    uint32_t vv;
    uint32_t ss;
    uint32_t region;
    uint32_t rem;
    uint32_t p;
    uint32_t q;
    uint32_t t;

    vv = (uint32_t)v * 255U / 100U;
    ss = (uint32_t)s * 255U / 100U;

    if(ss == 0U)
    {
        return (vv << 16) | (vv << 8) | vv;
    }

    region = (uint32_t)h / 60U;
    rem    = ((uint32_t)h % 60U) * 255U / 60U;

    p = vv * (255U - ss) / 255U;
    q = vv * (255U - (ss * rem / 255U)) / 255U;
    t = vv * (255U - (ss * (255U - rem) / 255U)) / 255U;

    switch(region)
    {
        case 0:  return (vv << 16) | (t << 8)  | p;
        case 1:  return (q << 16)  | (vv << 8) | p;
        case 2:  return (p << 16)  | (vv << 8) | t;
        case 3:  return (p << 16)  | (q << 8)  | vv;
        case 4:  return (t << 16)  | (p << 8)  | vv;
        default: return (vv << 16) | (p << 8)  | q;
    }
}

/* 当前相位（0..period 个 tick） */
static uint32_t phase_now(void)
{
    rt_tick_t per = rt_tick_from_millisecond(s_period);

    if(per == 0)
    {
        per = 1;
    }

    return (uint32_t)((rt_tick_get() - s_phase_t0) % per);
}

/* 算一帧并输出 */
static void led_fx_step(void)
{
    uint32_t rgb  = s_rgb;
    uint32_t duty = 100;

    /* 1. 渐变推进 */
    if(s_fading != 0U)
    {
        uint32_t el = (uint32_t)(rt_tick_get() - s_fade_t0);

        if(el >= (uint32_t)s_fade_ms)
        {
            s_rgb    = s_fade_to;
            s_fading = 0;
        }
        else
        {
            s_rgb = lerp_rgb(s_fade_from, s_fade_to, el * 1000U / (uint32_t)s_fade_ms);
        }

        rgb = s_rgb;
    }

    /* 2. 被 off() 关掉了 */
    if(s_off != 0U)
    {
        (void)led_set(0, 0);
        return;
    }

    /* 3. 按模式算这一帧 */
    switch(s_mode)
    {
        case M_BREATH:
        {
            uint32_t half = (uint32_t)(s_period / 2U);
            uint32_t ph   = phase_now();
            uint32_t tri;

            if(half == 0U)
            {
                half = 1U;
            }

            tri  = (ph < half) ? (ph * 1000U / half)
                               : ((uint32_t)(s_period - ph) * 1000U / half);
            duty = tri / 10U;                       /* 0..100 */
            break;
        }

        case M_BLINK:
            duty = (phase_now() < (uint32_t)(s_period / 2U)) ? 100U : 0U;
            break;

        case M_RAINBOW:
        {
            uint32_t per = (uint32_t)(s_period ? s_period : 1U);
            uint16_t hue = (uint16_t)(phase_now() * 360U / per);

            rgb = hsv2rgb(hue, 100, 100);
            break;
        }

        case M_STEADY:
        default:
            break;
    }

    (void)led_set(rgb, duty);
}

static void led_fx_timer_cb(void *param)
{
    (void)param;

    led_fx_step();

    /* 常亮且没有渐变在跑：定时器可以歇了 */
    if((s_mode == M_STEADY) && (s_fading == 0U) && (s_timer != RT_NULL))
    {
        (void)rt_timer_stop(s_timer);
    }
}

static void led_fx_timer_run(void)
{
    if(s_timer == RT_NULL)
    {
        s_timer = rt_timer_create("ledfx", led_fx_timer_cb, RT_NULL,
                                  rt_tick_from_millisecond(10), RT_TIMER_FLAG_PERIODIC);
    }

    if(s_timer != RT_NULL)
    {
        (void)rt_timer_start(s_timer);
    }
}

static void led_fx_timer_halt(void)
{
    if(s_timer != RT_NULL)
    {
        (void)rt_timer_stop(s_timer);
    }
}

/**************************************************************************************************
 *                                        对外接口
 **************************************************************************************************/
void led_fx_init(void)
{
    s_phase_t0 = rt_tick_get();
    s_off      = 0;
    s_fading   = 0;

    (void)led_set(s_rgb, 100);                  /* 顺便把 PWM 初始化掉，别让定时器里做 */
}

void led_fx_set_color(uint32_t rgb)
{
    if(rgb > 0xffffffU)
    {
        return;
    }

    s_fading = 0;                               /* 直接换色会打断渐变 */
    s_off    = 0;
    s_rgb    = rgb;

    led_fx_step();
}

void led_fx_set_color_index(uint8_t idx)
{
    led_fx_set_color(led_fx_palette[idx % LED_FX_PALETTE_NUM]);
}

void led_fx_next_color(void)
{
    uint8_t i;
    uint8_t cur = 0;

    for(i = 0; i < LED_FX_PALETTE_NUM; i++)
    {
        if(led_fx_palette[i] == s_rgb)
        {
            cur = i;
            break;
        }
    }

    led_fx_set_color_index((uint8_t)((cur + 1U) % LED_FX_PALETTE_NUM));
}

void led_fx_fade_to(uint32_t rgb, uint16_t ms)
{
    if(rgb > 0xffffffU)
    {
        return;
    }

    if(ms == 0U)
    {
        led_fx_set_color(rgb);
        return;
    }

    s_fade_from = s_rgb;
    s_fade_to   = rgb;
    s_fade_ms   = ms;
    s_fade_t0   = rt_tick_get();
    s_fading    = 1;
    s_off       = 0;

    led_fx_timer_run();                         /* 渐变靠定时器推进 */
}

void led_fx_steady(void)
{
    s_mode = M_STEADY;
    s_off  = 0;

    led_fx_step();

    if(s_fading == 0U)
    {
        led_fx_timer_halt();
    }
}

void led_fx_breath(uint16_t period_ms)
{
    s_mode     = M_BREATH;
    s_period   = (period_ms < 200U) ? 200U : period_ms;
    s_phase_t0 = rt_tick_get();
    s_off      = 0;

    led_fx_step();
    led_fx_timer_run();
}

void led_fx_blink(uint16_t period_ms)
{
    s_mode     = M_BLINK;
    s_period   = (period_ms < 100U) ? 100U : period_ms;
    s_phase_t0 = rt_tick_get();
    s_off      = 0;

    led_fx_step();
    led_fx_timer_run();
}

void led_fx_rainbow(uint16_t period_ms)
{
    s_mode     = M_RAINBOW;
    s_period   = (period_ms < 500U) ? 500U : period_ms;
    s_phase_t0 = rt_tick_get();
    s_off      = 0;

    led_fx_step();
    led_fx_timer_run();
}

void led_fx_off(void)
{
    s_off    = 1;
    s_mode   = M_STEADY;
    s_fading = 0;

    (void)led_set(0, 0);
    led_fx_timer_halt();
}

/* ============================================================================
 * 使用示例：整段复制到你的 task 里
 * ========================================================================== */
#if 0
#include "led_fx.h"

static void led_demo(void)
{
    led_fx_init();                      /* 进页面：推出默认红色常亮 */

    led_fx_next_color();                /* 切到队列里的下一个颜色 */
    led_fx_set_color_index(2);          /* 直接跳到第 3 个（蓝） */
    led_fx_fade_to(0xff8000, 300);      /* 300ms 渐变到琥珀 */

    led_fx_steady();                    /* 常亮：停止动态模式，定时器自己会停 */
    led_fx_breath(2000);                /* 2 秒一次呼吸，任务不用再管 */
    led_fx_blink(400);                  /* 0.2 秒亮 / 0.2 秒灭 */
    led_fx_rainbow(6000);               /* 6 秒转一圈彩虹 */

    led_fx_off();                       /* 退页面：熄灯并停掉定时器 */
}
#endif
