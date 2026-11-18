/******************************************************************************
 * 文件名   : menu_ui.c
 * 说明     : 选择页界面 —— 一屏摆完所有任务，每个任务一个按钮
 *
 * 本文件直接用 LVGL 画，不依赖任何封装层：
 *     页面       lv_obj_create(NULL) + 渐变底色
 *     标题栏/提示条/按钮   都是 lv_obj_create 出来的圆角卡片
 *     文字       lv_label_create + montserrat_14（只有 ASCII 可用）
 *
 * 版式（320×480 竖屏）：
 *     标题栏        TASKS / 共几个
 *     按钮墙        2 列 × 4 行，每个 140×70
 *     底部提示      TAP OR K3 TO ENTER
 *
 * 交互：
 *     点按钮     发 type=1 控件事件（编号 = 下标+1），menu.c 收到就进
 *     K1/K2      移动高亮，只改边框颜色，不重建对象
 *     K3         进当前高亮那个
 ******************************************************************************/

#include "menu_ui.h"
#include "theme.h"
#include "kernel.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
/* 一屏最多 8 个按钮：2 列 × 4 行 */
static lv_obj_t *s_btn[8];

static lv_obj_t *s_page = NULL;     /* NULL = 页面还没搭 */
static uint8_t   s_cnt  = 0;        /* 实际搭了几个按钮 */
static uint8_t   s_sel  = 0xFF;     /* 上次高亮的是哪个，0xFF = 还没画过 */

/**************************************************************************************************
 *                                        内部函数
 **************************************************************************************************/
/**
 * @brief  按钮点击：发一个 type=1 的控件事件，编号就是按钮序号
 * @note   user_data 里存的是 (下标 + 1)
 */
static void on_btn(lv_event_t *e)
{
    uint8_t id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    kernel_post(1, id, 0);                  /* 1 = 控件事件 */
}

/**
 * @brief  造一个带渐变底色的整屏
 */
static lv_obj_t *page_new(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);

    lv_obj_set_style_bg_color(scr, UI_C_BG, 0);
    lv_obj_set_style_bg_grad_color(scr, UI_C_BG_BOTTOM, 0);
    lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    return scr;
}

/**
 * @brief  造一张圆角卡片（标题栏 / 提示条 / 按钮都用它）
 */
static lv_obj_t *card_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, lv_coord_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, UI_C_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_C_BORDER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);

    return obj;
}

/**
 * @brief  造一段文字（透明底、无边框）
 */
static lv_obj_t *label_new(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);

    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lbl, 0, 0);

    return lbl;
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
        s_page = page_new();

        /* 1.1 标题栏：左边 TASKS，右边共几个 */
        bar = card_new(s_page, 12, 10, (lv_coord_t)(UI_W - 24),
                       (lv_coord_t)(UI_TITLE_H - 10), UI_RADIUS);
        lv_obj_set_style_bg_opa(bar, 242, 0);

        lbl = label_new(bar, UI_C_TEXT);
        lv_label_set_text(lbl, "TASKS");
        lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

        lbl = label_new(bar, UI_C_TEXT_DIM);
        snprintf(buf, sizeof(buf), "%d TASKS", count);
        lv_label_set_text(lbl, buf);
        lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

        /* 1.2 按钮墙：2 列（x = 16 / 168），行高 70 + 10 间距 */
        for(i = 0; i < count; i++)
        {
            x = (lv_coord_t)(16 + (i % 2) * 152);
            y = (lv_coord_t)(70 + (i / 2) * 80);

            btn = card_new(s_page, x, y, 140, 70, 18);
            lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(btn, on_btn, LV_EVENT_CLICKED, (void *)(uintptr_t)(i + 1));

            lbl = label_new(btn, UI_C_VALUE);
            lv_label_set_text(lbl, (names[i] != NULL) ? names[i] : "-");
            lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

            s_btn[i] = btn;
        }

        s_cnt = count;
        s_sel = 0xFF;

        /* 1.3 底部提示 */
        bar = card_new(s_page, 24, (lv_coord_t)(UI_H - 46),
                       (lv_coord_t)(UI_W - 48), 36, 18);
        lv_obj_set_style_bg_opa(bar, 215, 0);

        lbl = label_new(bar, UI_C_TEXT_DIM);
        lv_label_set_text(lbl, "TAP OR K3 TO ENTER");
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

    s_cnt = 0;
    s_sel = 0xFF;           /* 下次进来当"第一次"处理，重新搭一遍 */
}
