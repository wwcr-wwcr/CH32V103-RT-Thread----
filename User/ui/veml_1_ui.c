/* ============================================================
 * task_1_ui.c —— LED + VEML7700 页面
 *
 * 数据流：task_1.c 的 render 每 10ms 调一次本函数，
 *         三个参数就是那一帧要显示的值，本文件不碰硬件。
 * ============================================================ */

#include "veml_1_ui.h"
#include "common.h"

static lv_obj_t *s_page   = NULL;
static lv_obj_t *s_panel1 = NULL;
static lv_obj_t *s_text1  = NULL;       /* "LUX" 标签，固定文字 */
static lv_obj_t *s_text2  = NULL;       /* "STATE" 标签，固定文字 */
static lv_obj_t *s_text3  = NULL;       /* ON / OFF */
static lv_obj_t *s_text4  = NULL;       /* lux 数值 */
static lv_obj_t *s_btn1   = NULL;
static lv_obj_t *s_btn2   = NULL;

void custom_ui_render(uint16_t lux, uint8_t bright, uint8_t led_on)
{
    if(s_page == NULL)                  /* 第一次进来才建页面 */
    {
        s_page = ui_page_create();

        ui_title_bar(s_page, "LED_VEML7700", "1/1");

        s_panel1 = ui_panel(s_page, 16, 108, 288, 300, UI_C_CARD);

        /* 第一行：STATE / ON-OFF */
        s_text2 = ui_text(s_panel1, UI_FONT_BIG, UI_C_TEXT);
        ui_text_set(s_text2, "STATE");
        ui_text_align(s_text2, LV_ALIGN_TOP_LEFT, 24, 44);

        s_text3 = ui_text(s_panel1, UI_FONT_BIG, UI_C_VALUE);
        ui_text_set(s_text3, "OFF");
        ui_text_align(s_text3, LV_ALIGN_TOP_LEFT, 160, 44);

        /* 第二行：LUX / 数值 */
        s_text1 = ui_text(s_panel1, UI_FONT_BIG, UI_C_TEXT);
        ui_text_set(s_text1, "LUX");
        ui_text_align(s_text1, LV_ALIGN_TOP_LEFT, 24, 108);

        s_text4 = ui_text(s_panel1, UI_FONT_BIG, UI_C_VALUE);
        ui_text_set(s_text4, "0");
        ui_text_align(s_text4, LV_ALIGN_TOP_LEFT, 160, 108);

        /* 按钮：210 + 46 = 256，在 panel 的 300 之内 */
        s_btn1 = ui_button(s_panel1, 24,  210, 88, 46, "ON/OFF", 1);
        s_btn2 = ui_button(s_panel1, 176, 210, 88, 46, "QUIT",   2);
    }

    if(!ui_page_is_active(s_page))      /* 切回本页：load 上去 */
    {
        ui_page_load(s_page);
    }

    /* ---- 每帧更新：只有会变的东西放这里 ---- */
    ui_text_num(s_text4, (int32_t)lux, "lux");
    ui_text_set(s_text3, (led_on != 0) ? "ON" : "OFF");

    (void)bright;                       /* 暂时没显示，留着以后加进度条 */
}

void custom_ui_destroy(void)
{
    ui_page_destroy(&s_page);

    s_panel1 = NULL;
    s_text1  = NULL;
    s_text2  = NULL;
    s_text3  = NULL;
    s_text4  = NULL;
    s_btn1   = NULL;
    s_btn2   = NULL;
}
