/******************************************************************************
 * 文件名   : x_ui.c
 * 说明     : XXX 页面（只画界面，数据从 render 参数进来）
 ******************************************************************************/

#include "task_ex_ui.h"         //                     <=========别忘记修改这里
#include "theme.h"         /* 用 UI_C_* 颜色才需要 */
#include "common.h"        /* 用 ui_xxx_new 才需要 */
#include "lvgl/lvgl.h"     /* 【必须】，lv_obj_t / lv_scr_load 在这 */
#include <stdio.h>         /* 用 snprintf 才需要 */

/* 【必须】页面句柄。NULL = 还没搭；destroy 后必须置回 NULL */
static lv_obj_t *s_page = NULL;

/* 【你的】需要每帧更新的控件句柄，一个都不要多 */
static lv_obj_t *s_value = NULL;


/* ====================== 【必须】首次建页（只在 s_page==NULL 时进） ====================== */
static void page_build(void)
{
    s_page = ui_page_new();
    lv_obj_t *bar;
    lv_obj_t *lbl;
    /* 【你的】版式：标题栏、卡片、控件，一行一个 */
    /* 1. 标题栏 */
    bar = ui_card_new(s_page, 12, 10, (lv_coord_t)(UI_W - 24),
                      (lv_coord_t)(UI_TITLE_H - 10), UI_RADIUS);
    lv_obj_set_style_bg_opa(bar, 242, 0);

    lbl = ui_label_text(bar, UI_C_TEXT, "DEMO");
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, UI_PAD, 0);

    lbl = ui_label_text(bar, UI_C_TEXT_DIM, "100ms");
    lv_obj_align(lbl, LV_ALIGN_RIGHT_MID, -UI_PAD, 0);

    /* 【你的】把要更新的句柄存进 static */
    /* s_value = ui_label_at(s_page, UI_C_VALUE, "--", LV_ALIGN_BOTTOM_LEFT, 16, -12); */

    /* 【你的】底部提示（可选） */
    /* ui_hint_bar(s_page, "K4 EXIT"); */
}


/* ====================== 【必须】对外渲染（kernel 线程每 10ms 调一次） ====================== */
void x_ui_render(/* 【你的】参数：要显示的数据 */)
{
    if(s_page == NULL)                  /* 【必须】懒建页 */
    {
        page_build();
    }

    if(lv_scr_act() != s_page)          /* 【必须】切回本页时 load 上去 */
    {
        lv_scr_load(s_page);
    }

    /* 【你的】数据更新，只写"会变"的东西 */
    /* ui_label_set(s_value, "123"); */
    /* ui_bar_set(s_bar, v);        */
}


/* ====================== 【必须】销毁（关页时由 router 调） ====================== */
void x_ui_destroy(void)
{
    if(s_page == NULL)                  /* 【必须】幂等，router 可能重复调 */
    {
        return;
    }

    lv_obj_del(s_page);                 /* 【必须】把整页还给 LVGL 堆 */
    s_page = NULL;                      /* 【必须】置 NULL */

    /* 【你的】跟着置 NULL，别让 render 摸到已删除的对象 */
    s_value = NULL;
}
