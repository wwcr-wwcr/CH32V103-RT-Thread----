/* ==================== app_ui.c ==================== */
/* ============================================================
 * 文件名   : app_ui.c
 * 说明     : 由 LVGL 可视化设计器生成（基于 APP/common.h）
 * 版式     : 竖屏 320×480
 * ============================================================ */

/* 单独用头文件时打开下一行：#include "app_ui.h" */
#include "theme.h"
#include "LVGL_DEMO/LVGL_Demo.h"
#include "lvgl/lvgl.h"

#define APP_UI_SCREEN_CNT  1          /* 屏幕数：单文件编译时不用头文件 */
#include <stdio.h>                      /* 绑定形参转文字用 snprintf */

/* ---------------- 控件句柄（render 里更新这些） ---------------- */
static lv_obj_t *s_page[APP_UI_SCREEN_CNT] = {0};

/* ====================== Screen1 ====================== */
static lv_obj_t *w1 = NULL;   /* card7 (卡片) */
static lv_obj_t *w2 = NULL;   /* label1 (文字)  ← render 里更新 */
static lv_obj_t *w3 = NULL;   /* label2 (文字)  ← render 里更新 */
static lv_obj_t *w4 = NULL;   /* label8 (文字)  ← render 里更新 */
static lv_obj_t *w5 = NULL;   /* label9 (文字)  ← render 里更新 */

/* ---------------- 建页 ---------------- */
static void build_0(void)
{
    s_page[0] = ui_page_new();                 /* 建整屏 */
    w1 = ui_card_new(s_page[0], 32, 34, 240, 100, 22);
    w2 = ui_label_at(s_page[0], UI_C_TEXT, "veml7700", 56, 54);
    w3 = ui_label_at(s_page[0], UI_C_TEXT, "0", 176, 56);
    w4 = ui_label_at(s_page[0], UI_C_TEXT, "is_down", 56, 96);
    w5 = ui_label_at(s_page[0], UI_C_TEXT, "0", 176, 96);
}

void task_led_ui_render(uint8_t screen, float c, uint8_t is_down)
{
    if(screen >= APP_UI_SCREEN_CNT)
    {
        return;
    }

    if(s_page[screen] == NULL)              /* 首次进来：建页 */
    {
        switch(screen)
        {
            case 0: build_0(); break;   /* Screen1 */
            default: break;
        }
    }

    if(lv_scr_act() != s_page[screen])      /* 切回本页：load 上去 */
    {
        lv_scr_load(s_page[screen]);
    }

    if(screen == 0)                     /* 本屏绑定的形参 -> 控件 */
    {
        char buf[24];

        snprintf(buf, sizeof(buf), "%.2f", (double)(c));
        ui_label_set(w3, buf);
        snprintf(buf, sizeof(buf), "%u", (unsigned)(is_down));
        ui_label_set(w5, buf);
    }

    /* TODO: 在这里用上面的句柄更新数据，例如
     *       ui_label_set(w2, buf);
     */
}

void task_led_ui_destroy(void)
{
    uint8_t i;

    for(i = 0; i < APP_UI_SCREEN_CNT; i++)
    {
        if(s_page[i] != NULL)
        {
            lv_obj_del(s_page[i]);
            s_page[i] = NULL;
        }
    }

    w1 = NULL;
    w2 = NULL;
    w3 = NULL;
    w4 = NULL;
    w5 = NULL;
}