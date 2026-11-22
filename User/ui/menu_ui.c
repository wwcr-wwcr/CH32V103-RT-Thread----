/******************************************************************************
 * 文件名   : menu_ui.c
 * 说明     : 选择页界面 —— 一屏摆完所有任务，每个任务一个按钮 + 底部亮度滑条
 *
 * 控件全部来自 APP/common.h（ui_xxx_new），本文件只负责版式和文字：
 *     页面     ui_page_new()
 *     卡片     ui_card_new()      标题栏 / 提示条 / 滑条底座
 *     文字     ui_label_new()     标题 / 计数 / 提示
 *     按钮     ui_btn_new()       编号 1..N，点了自动投一条 TOUCH 事件
 *     滑条     ui_slider_new()    编号 200，松手才投一条
 *
 * 版式（320×480 竖屏）：
 *     标题栏        TASKS / 共几个
 *     按钮墙        2 列 × 4 行，每个 140×70
 *     亮度滑条      BRIGHT 0~100
 *     底部提示      TAP OR K3 TO ENTER
 *
 * 滑条为什么只刷数字、不投递：
 *     LV_EVENT_VALUE_CHANGED 拖动时手指每动一点就触发一次，一秒钟几十条，
 *     每次都 kernel_post 会把 kernel 的队列（深度 8）灌满，按键事件被挤掉。
 *     所以：拖动中只改本页数字（本文件的 on_slide_move），
 *           松手由 ui_slider_new 内部统一投一条最终值。
 ******************************************************************************/

#include "menu_ui.h"
#include "theme.h"
#include "kernel.h"
#include "common.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
/* 一屏最多 8 个按钮：2 列 × 4 行 */
static lv_obj_t *s_btn[8];

static lv_obj_t *s_page    = NULL;      /* NULL = 页面还没搭 */
static uint8_t   s_cnt     = 0;         /* 实际搭了几个按钮 */
static uint8_t   s_sel     = 0xFF;      /* 上次高亮的是哪个，0xFF = 还没画过 */

static lv_obj_t *s_slider  = NULL;      /* 亮度滑条 */
static lv_obj_t *s_bright  = NULL;      /* 滑条右边的数值 */
static uint8_t   s_percent = 50;        /* 亮度值，跨页面保留 */

/**************************************************************************************************
 *                                        内部函数
 **************************************************************************************************/
/**
 * @brief  滑条拖动中：只更新数字，一条消息都不发
 * @note   松手发消息由 ui_slider_new 内部负责，这里只叠一个"实时数字"回调
 */
static void on_slide_move(lv_event_t *e)
{
    char buf[8];

    s_percent = (uint8_t)lv_slider_get_value(lv_event_get_target(e));

    snprintf(buf, sizeof(buf), "%u", (unsigned)s_percent);
    ui_label_set(s_bright, buf);
}

/**
 * @brief  搭底部的亮度滑条（卡片 + 文字 + 滑条）
 */
static void bright_build(void)
{
    lv_obj_t *box;
    lv_obj_t *lbl;
    char      buf[8];

    box = ui_card_new(s_page, 16, (lv_coord_t)(UI_H - 106), (lv_coord_t)(UI_W - 32), 52, 14);

    lbl = ui_label_text(box, UI_C_TEXT_DIM, "BRIGHT");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 12, 6);

    s_bright = ui_label_new(box, UI_C_VALUE);
    snprintf(buf, sizeof(buf), "%u", (unsigned)s_percent);
    ui_label_set(s_bright, buf);
    lv_obj_align(s_bright, LV_ALIGN_TOP_RIGHT, -12, 6);

    s_slider = ui_slider_new(box, 12, 28, (lv_coord_t)(UI_W - 56), 8,
                             0, 100, s_percent, MENU_ID_BRIGHT);

    /* 再叠一个"拖动时刷新数字"的回调（不发消息） */
    lv_obj_add_event_cb(s_slider, on_slide_move, LV_EVENT_VALUE_CHANGED, NULL);
}

/**************************************************************************************************
 *                                        对外函数
 **************************************************************************************************/
void menu_ui_render(const char **names, uint8_t count, uint8_t sel)
{
    lv_obj_t   *bar;
    lv_obj_t   *lbl;
    lv_obj_t   *btn;
    lv_coord_t  x;
    lv_coord_t  y;
    uint8_t     i;
    char        buf[20];

    if(count > (uint8_t)(sizeof(s_btn) / sizeof(s_btn[0])))
    {
        count = (uint8_t)(sizeof(s_btn) / sizeof(s_btn[0]));     /* 超了就截断，别越界 */
    }

    /* 1. 第一次（或从任务页退回来）：搭界面 */
    if(s_page == NULL)
    {
        s_page = ui_page_new();

        /* 1.1 标题栏：左边 TASKS，右边共几个 */
        bar = ui_card_new(s_page, 12, 10, (lv_coord_t)(UI_W - 24),
                          (lv_coord_t)(UI_TITLE_H - 10), UI_RADIUS);
        lv_obj_set_style_bg_opa(bar, 242, 0);

        lbl = ui_label_text(bar, UI_C_TEXT, "TASKS");
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

        snprintf(buf, sizeof(buf), "%d TASKS", count);
        lbl = ui_label_text(bar, UI_C_TEXT_DIM, buf);
        lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

        /* 1.2 按钮墙：2 列（x = 16 / 168），行距 76，最多 4 行 */
        for(i = 0; i < count; i++)
        {
            x = (lv_coord_t)(16 + (i % 2) * 152);
            y = (lv_coord_t)(66 + (i / 2) * 76);

            btn = ui_btn_new(s_page, x, y, 140, 70,
                             (names[i] != NULL) ? names[i] : "-", (uint8_t)(i + 1));

            s_btn[i] = btn;
        }

        s_cnt = count;
        s_sel = 0xFF;

        /* 1.3 亮度滑条 */
        bright_build();

        /* 1.4 底部提示 */
        bar = ui_card_new(s_page, 24, (lv_coord_t)(UI_H - 46), (lv_coord_t)(UI_W - 48), 36, 18);
        lv_obj_set_style_bg_opa(bar, 215, 0);

        lbl = ui_label_text(bar, UI_C_TEXT_DIM, "TAP OR K3 TO ENTER");
        lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
    }

    /* 页面刚搭好（还不是当前屏）：load 上去 */
    if(lv_scr_act() != s_page)
    {
        lv_scr_load(s_page);
    }

    /* 2. 高亮没变就不动，省一次重绘 */
    if(sel == s_sel)
    {
        return;
    }

    /* 3. 只改边框和底色，不重建对象 */
    for(i = 0; i < s_cnt; i++)
    {
        uint8_t on = (i == sel) ? 1 : 0;

        lv_obj_set_style_border_width(s_btn[i], (on != 0) ? 3 : 1, 0);
        lv_obj_set_style_border_color(s_btn[i], (on != 0) ? UI_C_ACCENT : UI_C_BORDER, 0);
        lv_obj_set_style_bg_color(s_btn[i], (on != 0) ? UI_C_HINT : UI_C_CARD, 0);
    }

    s_sel = sel;
}

void menu_ui_destroy(void)
{
    uint8_t i;

    if(s_page == NULL)
    {
        return;
    }

    /* 这时候页面已经不是当前屏了（router 先把新界面画完才删的），直接删 */
    lv_obj_del(s_page);
    s_page = NULL;

    for(i = 0; i < (uint8_t)(sizeof(s_btn) / sizeof(s_btn[0])); i++)
    {
        s_btn[i] = NULL;
    }

    s_slider = NULL;
    s_bright = NULL;
    s_cnt    = 0;
    s_sel    = 0xFF;        /* 下次进来当"第一次"处理，重新搭一遍 */
}
