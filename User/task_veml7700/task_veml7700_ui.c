/* ============================================================
 * 文件名   : task_veml7700.c
 * 说明     : 由 LVGL 可视化设计器生成（基于 APP/LVGL_DEMO/LVGL_Demo.h）
 * 版式     : 竖屏 320×480
 * ============================================================ */

/* 单独用头文件时打开下一行：#include "task_veml7700.h" */
#include "theme.h"
#include "LVGL_DEMO/LVGL_Demo.h"
#include "lvgl/lvgl.h"

#include <stdio.h>                      /* 绑定形参转文字用 snprintf */

/* ---------------- 控件句柄（render 里更新这些） ---------------- */
static lv_obj_t *s_page = NULL;

/* ====================== Screen1 ====================== */
static lv_obj_t *w1 = NULL;   /* label1 (文字)  ← render 里更新 */

/* ---------------- 建页 ---------------- */
static void build_0(void)
{
    s_page = ui_page_new();                        /* 建整屏 */
    w1 = ui_label_at(s_page, UI_C_TEXT, "TEXT", 120, 128);
}

void task_veml7700_render(float c)
{
    if(s_page == NULL)                      /* 首次进来：建页 */
    {
        build_0();
    }

    if(lv_scr_act() != s_page)              /* 切回本页：load 上去 */
    {
        lv_scr_load(s_page);
    }

    char buf[24];

    snprintf(buf, sizeof(buf), "%.2f", (double)(c));
    ui_label_set(w1, buf);

    /* TODO: 在这里用上面的句柄更新数据，例如
     *       ui_label_set(w1, buf);
     */
}

void task_veml7700_destroy(void)
{
    if(s_page != NULL)
    {
        lv_obj_del(s_page);
        s_page = NULL;
    }

    w1 = NULL;
}
