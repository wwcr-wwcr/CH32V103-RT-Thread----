/******************************************************************************
 * 文件名   : task_example_ui.c
 * 说明     : EXAMPLE 页面 —— 计数演示（直接用 LVGL 画，不依赖封装层）
 *
 * 版式（320×480 竖屏）：
 *     标题栏      DEMO / 100ms
 *     数值卡      COUNT 大字
 *     STATE       行
 *     EVENTS      行
 *     三个按钮    +10 / CLR / QUIT
 *     底部提示    K1 RUN  K2 +10  K3 CLR
 *
 * 本文件只画界面：数据由任务的 render 传进来，按钮点了只发 type=1 事件。
 ******************************************************************************/

#include "task_example_ui.h"
#include "theme.h"
#include "kernel.h"
#include "lvgl/lvgl.h"
#include <stdio.h>

/**************************************************************************************************
 *                                        内部变量
 **************************************************************************************************/
static lv_obj_t *s_page   = NULL;       /* NULL = 页面还没搭 */
static lv_obj_t *s_value  = NULL;       /* COUNT 的数值 */
static lv_obj_t *s_state  = NULL;       /* RUN / PAUSE */
static lv_obj_t *s_events = NULL;       /* 操作次数 */

/**************************************************************************************************
 *                                        内部函数
 **************************************************************************************************/
/**
 * @brief  按钮点击：发 type=1 控件事件，编号就是按钮序号
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
 * @brief  造一张圆角卡片（标题栏 / 提示条 / 数值卡 / 按钮都用它）
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

/**
 * @brief  造一个按钮：点一下发 type=1 事件
 */
static void btn_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                    const char *text, uint8_t id)
{
    lv_obj_t *btn = card_new(parent, x, y, 88, 52, 18);
    lv_obj_t *lbl;

    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(btn, on_btn, LV_EVENT_CLICKED, (void *)(uintptr_t)id);

    lbl = label_new(btn, UI_C_VALUE);
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);
}

/**
 * @brief  一行"标签 + 数值"，返回数值 label
 */
static lv_obj_t *row_new(lv_obj_t *parent, lv_coord_t y, const char *text)
{
    lv_obj_t *lbl;

    lbl = label_new(parent, UI_C_TEXT_DIM);
    lv_label_set_text(lbl, text);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 24, y);

    lbl = label_new(parent, UI_C_VALUE);
    lv_label_set_text(lbl, "-");
    lv_obj_align(lbl, LV_ALIGN_TOP_RIGHT, -24, y);

    return lbl;
}

/**************************************************************************************************
 *                                        内部：搭页面
 **************************************************************************************************/
static void page_build(void)
{
    lv_obj_t *bar;
    lv_obj_t *lbl;
    lv_obj_t *box;

    s_page = page_new();

    /* 1. 标题栏 */
    bar = card_new(s_page, 12, 10, (lv_coord_t)(UI_W - 24),
                   (lv_coord_t)(UI_TITLE_H - 10), UI_RADIUS);
    lv_obj_set_style_bg_opa(bar, 242, 0);

    lbl = label_new(bar, UI_C_TEXT);
    lv_label_set_text(lbl, "DEMO");
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

    lbl = label_new(bar, UI_C_TEXT_DIM);
    lv_label_set_text(lbl, "100ms");
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

    /* 2. 数值卡：标题 + 大字 */
    box = card_new(s_page, 20, 64, 280, 132, UI_RADIUS);

    lbl = label_new(box, UI_C_TEXT_DIM);
    lv_label_set_text(lbl, "COUNT");
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 16, 14);

    s_value = label_new(box, UI_C_VALUE);
    lv_label_set_text(s_value, "0");
    lv_obj_align(s_value, LV_ALIGN_BOTTOM_LEFT, 16, -14);

    /* 3. 两行状态 */
    s_state  = row_new(s_page, 214, "STATE");
    s_events = row_new(s_page, 252, "EVENTS");

    /* 4. 三个触摸按钮（编号 1 / 2 / 3 由任务侧解释） */
    btn_new(s_page,  16, 300, "+10",  1);
    btn_new(s_page, 116, 300, "CLR",  2);
    btn_new(s_page, 216, 300, "QUIT", 3);

    /* 5. 底部提示 */
    bar = card_new(s_page, 24, (lv_coord_t)(UI_H - 46),
                   (lv_coord_t)(UI_W - 48), 36, 18);
    lv_obj_set_style_bg_opa(bar, 215, 0);

    lbl = label_new(bar, UI_C_TEXT_DIM);
    lv_label_set_text(lbl, "K1 RUN  K2 +10  K3 CLR");
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
    lv_label_set_text(s_value, buf);

    lv_label_set_text(s_state, (run != 0) ? "RUN" : "PAUSE");

    snprintf(buf, sizeof(buf), "%u", (unsigned)events);
    lv_label_set_text(s_events, buf);
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
}
