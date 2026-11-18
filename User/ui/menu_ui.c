/******************************************************************************
 * 文件名   : menu_ui.c
 * 说明     : 选择页界面：一屏一个 TASK n，左右切换
 *
 * 触摸支持：
 *     左右两个箭头按钮 + 卡片本身都可以点。回调只做"合成按键事件"，
 *     和实体按键 K1/K2/K3 走完全相同的路径，菜单逻辑一行都不用改。
 *
 * 生命周期（kernel 管）：
 *     进任务    kernel 调 menu_ui_destroy()，页面还给 LVGL 堆
 *     回首页    menu_ui_render() 发现 s_card 是 NULL，自己重新搭一遍
 ******************************************************************************/

#include "menu_ui.h"
#include "common.h"
#include <stdio.h>

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
static void (*s_cb_prev)(void)  = NULL;
static void (*s_cb_next)(void)  = NULL;
static void (*s_cb_enter)(void) = NULL;

/* 页面对象放在文件作用域，menu_ui_destroy() 才够得着。
 * 全是 NULL 就表示"这一页还没搭"，render 里靠 s_card 判断。 */
static lv_obj_t *s_page    = NULL;
static lv_obj_t *s_pagebar = NULL;
static lv_obj_t *s_card    = NULL;
static lv_obj_t *s_task    = NULL;
static lv_obj_t *s_name    = NULL;
static uint8_t   s_last    = 0xFF;      /* 0xFF = 还没画过任何一项 */

/**************************************************************************************************
 *                                        内部函数
 **************************************************************************************************/
static void on_prev(lv_event_t *e)  { (void)e; if(s_cb_prev  != NULL) { s_cb_prev();  } }
static void on_next(lv_event_t *e)  { (void)e; if(s_cb_next  != NULL) { s_cb_next();  } }
static void on_enter(lv_event_t *e) { (void)e; if(s_cb_enter != NULL) { s_cb_enter(); } }

/* 造一个方形触摸按钮：深色卡片 + 边框 + 居中箭头 */
static lv_obj_t *make_btn(lv_obj_t *parent, lv_coord_t x, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = ui_panel(parent, x, 335, 56, 56, UI_C_CARD);
    lv_obj_t *lbl;

    lv_obj_set_style_border_width(btn, 2, 0);
    lv_obj_set_style_border_color(btn, UI_C_BORDER, 0);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lbl = ui_text(btn, UI_FONT_HUGE, UI_C_TEXT);
    ui_text_set(lbl, text);
    ui_text_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}

/**************************************************************************************************
 *                                        对外函数
 **************************************************************************************************/
void menu_ui_set_tap_cb(void (*on_prev_cb)(void), void (*on_next_cb)(void), void (*on_enter_cb)(void))
{
    s_cb_prev  = on_prev_cb;
    s_cb_next  = on_next_cb;
    s_cb_enter = on_enter_cb;
}

void menu_ui_render(uint8_t index, uint8_t total, const char *name)
{
    char   buf[24];

    /* 1. 首次进入（或从任务页退回来）：搭界面 */
    if(s_card == NULL)
    {
        lv_obj_t *root;

        s_page = ui_page_create();
        root = s_page;

        s_pagebar = ui_title_bar(root, "SELECT", "1/1");

        /* 卡片：点一下 = 进入当前任务 */
        s_card = ui_panel(root, 30, 165, (lv_coord_t)(UI_W - 60), 150, UI_C_CARD);
        lv_obj_set_style_border_width(s_card, 2, 0);
        lv_obj_set_style_border_color(s_card, UI_C_BORDER, 0);
        lv_obj_add_flag(s_card, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s_card, on_enter, LV_EVENT_CLICKED, NULL);

        s_task = ui_text(s_card, UI_FONT_HUGE, UI_C_TEXT);
        ui_text_align(s_task, LV_ALIGN_TOP_MID, 0, 26);

        s_name = ui_text(s_card, UI_FONT_BIG, UI_C_ACCENT);
        ui_text_align(s_name, LV_ALIGN_BOTTOM_MID, 0, -26);

        /* 左右箭头：点一下 = 上一个 / 下一个 */
        make_btn(root, 24, "<", on_prev);
        make_btn(root, (lv_coord_t)(UI_W - 24 - 56), ">", on_next);

        ui_hint_bar(root, "TAP CARD TO ENTER");
    }

    /* 页面刚搭好（还不是当前屏）：load 上去 */
    if(!ui_page_is_active(s_page))
    {
        ui_page_load(s_page);
    }

    /* 2. 没变化就不动，省一次重绘 */
    if(index == s_last)
    {
        return;
    }

    /* 3. 更新文字 */
    snprintf(buf, sizeof(buf), "TASK %d", index + 1);
    ui_text_set(s_task, buf);
    ui_text_set(s_name, name);

    snprintf(buf, sizeof(buf), "%d/%d", index + 1, total);
    ui_text_set(s_pagebar, buf);

    s_last = index;
}

void menu_ui_destroy(void)
{
    /* 这时候页面已经不是当前屏了（kernel 先把下一个界面画完才删的），直接删就行 */
    ui_page_destroy(&s_page);

    s_pagebar = NULL;
    s_card    = NULL;
    s_task    = NULL;
    s_name    = NULL;
    s_last    = 0xFF;           /* 下次进来当"第一次"处理，文字会重新填 */
}
