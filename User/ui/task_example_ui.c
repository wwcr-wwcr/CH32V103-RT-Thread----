/******************************************************************************
 * 文件名   : task_example_ui.c
 * 说明     : EXAMPLE 页面 —— 计数演示
 *
 * 控件全部来自 APP/common.h（ui_xxx_new），本文件只负责版式和刷数字：
 *     页面     ui_page_new()
 *     卡片     ui_card_new()      标题栏 / 数值卡 / 提示条
 *     文字     ui_label_new()     标签 + 数值
 *     按钮     ui_btn_new()       编号 1/2/3，点了自动投一条 TOUCH 事件
 *     进度条   ui_bar_new()       count % 100
 *
 * 版式（320×480 竖屏）：
 *     标题栏      DEMO / 100ms
 *     数值卡      COUNT 大字 + 进度条
 *     STATE       行
 *     EVENTS      行
 *     三个按钮    +10 / CLR / QUIT
 *     底部提示    K1 RUN  K2 +10  K3 CLR
 ******************************************************************************/

#include "task_example_ui.h"
#include "theme.h"
#include "common.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
static lv_obj_t *s_page   = NULL;       /* NULL = 页面还没搭 */
static lv_obj_t *s_value  = NULL;       /* COUNT 的数值 */
static lv_obj_t *s_state  = NULL;       /* RUN / PAUSE */
static lv_obj_t *s_events = NULL;       /* 操作次数 */
static lv_obj_t *s_bar    = NULL;       /* count % 100 的进度条 */

/**************************************************************************************************
 *                                        内部：搭页面
 **************************************************************************************************/
/**
 * @brief  一行"标签 + 数值"，返回数值 label
 */
static lv_obj_t *row_new(lv_obj_t *parent, lv_coord_t y, const char *text)
{
    lv_obj_t *lbl;

    lbl = ui_label_text(parent, UI_C_TEXT_DIM, text);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 24, y);

    lbl = ui_label_text(parent, UI_C_VALUE, "-");
    lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -24, y);

    return lbl;
}

static void page_build(void)
{
    lv_obj_t *bar;
    lv_obj_t *lbl;
    lv_obj_t *box;

    s_page = ui_page_new();

    /* 1. 标题栏 */
    bar = ui_card_new(s_page, 12, 10, (lv_coord_t)(UI_W - 24),
                      (lv_coord_t)(UI_TITLE_H - 10), UI_RADIUS);
    lv_obj_set_style_bg_opa(bar, 242, 0);

    lbl = ui_label_text(bar, UI_C_TEXT, "DEMO");
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

    lbl = ui_label_text(bar, UI_C_TEXT_DIM, "100ms");
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

    /* 2. 数值卡：标题 + 大字 + 进度条 */
    box = ui_card_new(s_page, 20, 64, 280, 132, UI_RADIUS);

    lbl = ui_label_text(box, UI_C_TEXT_DIM, "COUNT");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 16, 14);

    s_value = ui_label_text(box, UI_C_VALUE, "0");
    lv_obj_align(s_value, LV_ALIGN_TOP_RIGHT, -16, 10);

    s_bar = ui_bar_new(box, 16, 96, 248, 10, 0, 100);

    /* 3. 两行状态 */
    s_state  = row_new(s_page, 214, "STATE");
    s_events = row_new(s_page, 252, "EVENTS");

    /* 4. 三个触摸按钮（编号 1 / 2 / 3 由任务侧解释） */
    (void)ui_btn_new(s_page,  16, 300, 88, 52, "+10",  1);
    (void)ui_btn_new(s_page, 116, 300, 88, 52, "CLR",  2);
    (void)ui_btn_new(s_page, 216, 300, 88, 52, "QUIT", 3);

    /* 5. 底部提示 */
    bar = ui_card_new(s_page, 24, (lv_coord_t)(UI_H - 46),
                      (lv_coord_t)(UI_W - 48), 36, 18);
    lv_obj_set_style_bg_opa(bar, 215, 0);

    lbl = ui_label_text(bar, UI_C_TEXT_DIM, "K1 RUN  K2 +10  K3 CLR");
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

/**************************************************************************************************
 *                                        对外函数
 **************************************************************************************************/
void demo_ui_render(uint32_t count, uint8_t run, uint16_t events)
{
    char buf[24];

    if(s_page == NULL)
    {
        page_build();
    }

    if(lv_scr_act() != s_page)
    {
        lv_scr_load(s_page);
    }

    /* 只有会变的东西放这，固定文字在 page_build 里已经设好 */
    snprintf(buf, sizeof(buf), "%lu", (unsigned long)count);
    ui_label_set(s_value, buf);

    ui_bar_set(s_bar, (int32_t)(count % 100U));

    ui_label_set(s_state, (run != 0) ? "RUN" : "PAUSE");

    snprintf(buf, sizeof(buf), "%u", (unsigned)events);
    ui_label_set(s_events, buf);
}

void demo_ui_destroy(void)
{
    if(s_page == NULL)
    {
        return;
    }

    lv_obj_del(s_page);
    s_page = NULL;

    s_value  = NULL;
    s_state  = NULL;
    s_events = NULL;
    s_bar    = NULL;
}
