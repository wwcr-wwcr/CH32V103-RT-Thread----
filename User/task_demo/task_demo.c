/******************************************************************************
 * 文件名   : task_demo.c
 * 说明     : LVGL 演示页 —— 纯界面，不开线程、不用队列
 *
 * K1 / K2 切换四种图表形态（OSC / SPECTRUM / COORD / SINGLE）。
 *
 * 因为没有线程要管，open() 只复位状态、close() 空着；
 * 按键由 post() 直接改状态，真正的建页/刷新都在 render()（kernel 线程）里做。
 ******************************************************************************/

#include "router.h"
#include "key.h"
#include "theme.h"
#include "LVGL_DEMO/LVGL_Demo.h"
#include "LVGL_DEMO/demo_chart.h"
#include "rtthread.h"

//********************在这里写常用变量********************//
static lv_obj_t *s_page  = NULL;        /* 整页 */
static lv_obj_t *s_name  = NULL;        /* 标题栏右边那个名字 */
static uint8_t   s_kind  = 0;           /* 当前第几种图表 */
static uint8_t   s_dirty = 1;           /* 1 = 需要重建图表对象 */
static rt_tick_t s_last  = 0;           /* 数据刷新节拍 */


static void demo_open(void)
{
//********************在这里写进入逻辑********************//
    s_kind  = 0;
    s_dirty = 1;
    s_last  = rt_tick_get();
}


static void demo_close(void)
{
//********************在这里写退出逻辑********************//
    /* 没有线程、没有队列、没有 PWM，什么都不用停 */
}


static void demo_post(const message *msg)
{
    if(msg->type != KEY)
    {
        return;
    }

    if(msg->code == KEY_ID_1)                   /* K1 上一种 */
    {
        s_kind  = (uint8_t)((s_kind + DEMO_CHART_NUM - 1U) % DEMO_CHART_NUM);
        s_dirty = 1;
    }
    else if(msg->code == KEY_ID_2)              /* K2 下一种 */
    {
        s_kind  = (uint8_t)((s_kind + 1U) % DEMO_CHART_NUM);
        s_dirty = 1;
    }
}


static void demo_ui_destroy(void)
{
    demo_chart_destroy();                       /* 先删图表，再删页 */

    if(s_page != NULL)
    {
        lv_obj_del(s_page);
        s_page = NULL;
        s_name = NULL;
    }

    s_dirty = 1;                                /* 下次进来重建 */
}


static void demo_render(void)
{
    lv_obj_t *bar;
    lv_obj_t *lbl;

    /* 1. 首次进来：搭页面骨架（标题栏 + 底部提示） */
    if(s_page == NULL)
    {
        s_page = ui_page_new();

        bar = ui_card_new(s_page, 12, 10, UI_W - 24, UI_TITLE_H - 10, UI_RADIUS);
        lv_obj_set_style_bg_opa(bar, 242, 0);

        lbl = ui_label_text(bar, UI_C_TEXT, "LVGL DEMO");
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

        s_name = ui_label_text(bar, UI_C_VALUE, demo_chart_name(DEMO_CHART_OSC));
        lv_obj_align(s_name, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

        bar = ui_card_new(s_page, 24, UI_H - 46, UI_W - 48, 36, 18);
        lv_obj_set_style_bg_opa(bar, 215, 0);

        lbl = ui_label_text(bar, UI_C_TEXT_DIM, "K1 / K2 SWITCH");
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

        s_dirty = 1;
    }

    if(lv_scr_act() != s_page)
    {
        lv_scr_load(s_page);
    }

    /* 2. 切换形态时才重建图表（删旧建新） */
    if(s_dirty != 0)
    {
        demo_chart_create(s_page, (demo_chart_t)s_kind);

        if(s_name != NULL)
        {
            ui_label_set(s_name, demo_chart_name((demo_chart_t)s_kind));
        }

        s_dirty = 0;
    }

    /* 3. 30ms 推一次数据（不跟着 10ms 渲染节拍跑，省 CPU） */
    if((rt_tick_get() - s_last) >= rt_tick_from_millisecond(30))
    {
        s_last = rt_tick_get();
        demo_chart_reflash();
    }
}


/**************************************************************************************************
 *                                        任务描述
 **************************************************************************************************/
const task_t task_demo =
{
    .name       = "DEMO",           /* 线程名 */
    .keep       = 0,                /* 1 = 前台 */

    .open       = demo_open,        /* 进入 */
    .close      = demo_close,       /* 退出 */
    .post       = demo_post,        /* 事件收取 */
    .render     = demo_render,      /* 接管屏幕刷新 */
    .ui_destroy = demo_ui_destroy,  /* lvgl 卸载 */
};
