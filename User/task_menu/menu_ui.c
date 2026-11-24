/******************************************************************************
 * 文件名   : menu_ui.c
 * 说明     : 选择页 —— 一屏任务按钮，每屏最多 8 个（2 列 x 4 行）
 *
 * 控件全部来自 APP/common.h 的 ui_xxx_new()，本文件只负责版式和文字：
 *     页面     ui_page_new()
 *     卡片     ui_card_new()      标题栏 / 底部提示
 *     文字     ui_label_text()    标题 / 计数 / 提示
 *     按钮     ui_btn_new()       编号 1..N，点击自动投一次 TOUCH 事件
 *
 * 版式（320x480 竖屏）：
 *     标题栏        TASKS / 任务数
 *     按钮墙        2 列 x N 行，每格 140x70，从 y=66 起、行距 76
 *     底部提示      TAP OR K3 TO ENTER
 *
 * 刷新策略：
 *     只有"选中项变了"才重画边框和底色，不动控件树；
 *     拖动/点击的实时反馈由控件自己处理。
******************************************************************************/

#include "menu_ui.h"
#include "theme.h"
#include "kernel.h"
#include "LVGL_DEMO/LVGL_Demo.h"
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

        /* 1.3 底部提示 */
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

    s_cnt    = 0;
    s_sel    = 0xFF;        /* 下次进来当"第一次"处理，重新搭一遍 */
}
