/******************************************************************************
 * 文件名   : common.c
 * 说明     : LVGL 控件封装 —— 统一风格的"造控件"函数（声明见 common.h）
 *
 * 这里只做两件事：
 *     1. 把"圆角 + 配色 + 边框 + 内边距"这套样式集中写一次
 *     2. 把"点一下 → kernel_post(TOUCH, id, 值)"也集中写一次
 * 界面文件从此只关心版式（谁放哪、什么文字），不再重复这十几行。
 ******************************************************************************/

#include "common.h"
#include "ui/theme.h"
#include "kernel.h"
#include "event.h"

/**************************************************************************************************
 *                                        内部：样式与回调
 **************************************************************************************************/
/* 卡片外观：圆角、卡片底色、细边框、无内边距、不滚动 */
static void style_card(lv_obj_t *obj, lv_coord_t radius)
{
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, UI_C_CARD, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, UI_C_BORDER, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

#if (LV_USE_BTN || LV_USE_SWITCH || LV_USE_CHECKBOX || LV_USE_SLIDER || LV_USE_ARC || \
     LV_USE_DROPDOWN || LV_USE_ROLLER || LV_USE_BTNMATRIX || LV_USE_TEXTAREA || \
     LV_USE_KEYBOARD || LV_USE_CALENDAR)

/* 回调里取"控件编号"：注册时 user_data 存的就是它 */
static uint8_t ev_id(lv_event_t *e)
{
    return (uint8_t)(uintptr_t)lv_event_get_user_data(e);
}
#endif

#if LV_USE_BTN
/* 按钮：点一下，value 固定 0 */
static void cb_click(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), 0);
}
#endif

#if LV_USE_SWITCH || LV_USE_CHECKBOX
/* 开关 / 复选框：value = 1 开，0 关 */
static void cb_onoff(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);

    kernel_post(TOUCH, ev_id(e), lv_obj_has_state(obj, LV_STATE_CHECKED) ? 1 : 0);
}
#endif

#if LV_USE_SLIDER
/* 滑条：只有松手才发，拖动过程一条都不发 */
static void cb_slider_done(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), (int32_t)lv_slider_get_value(lv_event_get_target(e)));
}
#endif

#if LV_USE_ARC
/* 圆弧：同样只在松手时发 */
static void cb_arc_done(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), (int32_t)lv_arc_get_value(lv_event_get_target(e)));
}
#endif

#if LV_USE_DROPDOWN
/* 下拉框：value = 选中项下标 */
static void cb_dropdown(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), (int32_t)lv_dropdown_get_selected(lv_event_get_target(e)));
}
#endif

#if LV_USE_ROLLER
/* 滚轮：value = 选中项下标 */
static void cb_roller(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), (int32_t)lv_roller_get_selected(lv_event_get_target(e)));
}
#endif

#if LV_USE_BTNMATRIX
/* 按钮矩阵：value = 按下第几个 */
static void cb_btnmatrix(lv_event_t *e)
{
    kernel_post(TOUCH, ev_id(e), (int32_t)lv_btnmatrix_get_selected_btn(lv_event_get_target(e)));
}
#endif

#if LV_USE_TEXTAREA || LV_USE_KEYBOARD
/* 文本框 / 键盘：确认 = 1，取消 = 0 */
static void cb_ready(lv_event_t *e)  { kernel_post(TOUCH, ev_id(e), 1); }
static void cb_cancel(lv_event_t *e) { kernel_post(TOUCH, ev_id(e), 0); }
#endif

#if LV_USE_CALENDAR
/* 日历：value = 点中的"日"（1..31），够用；要年月再自己取 */
static void cb_calendar(lv_event_t *e)
{
    lv_calendar_date_t d;

    if(lv_calendar_get_pressed_date(lv_event_get_target(e), &d) == LV_RES_OK)
    {
        kernel_post(TOUCH, ev_id(e), (int32_t)d.day);
    }
}
#endif

/**************************************************************************************************
 *                                        基础：页面 / 卡片 / 文字
 **************************************************************************************************/
lv_obj_t *ui_page_new(void)
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

lv_obj_t *ui_card_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h, lv_coord_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    style_card(obj, radius);

    return obj;
}

lv_obj_t *ui_label_new(lv_obj_t *parent, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);

    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_style_bg_opa(lbl, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(lbl, 0, 0);

    return lbl;
}

lv_obj_t *ui_label_text(lv_obj_t *parent, lv_color_t color, const char *text)
{
    lv_obj_t *lbl = ui_label_new(parent, color);

    lv_label_set_text(lbl, (text != NULL) ? text : "");

    return lbl;
}

void ui_label_set(lv_obj_t *lbl, const char *text)
{
    if(lbl != NULL)
    {
        lv_label_set_text(lbl, (text != NULL) ? text : "");
    }
}

/**************************************************************************************************
 *                                        交互类
 **************************************************************************************************/
#if LV_USE_BTN
lv_obj_t *ui_btn_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, const char *text, uint8_t id)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl;

    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    style_card(btn, 18);
    lv_obj_set_style_shadow_width(btn, 0, 0);       /* 主题给按钮加了阴影，去掉 */

    lv_obj_add_event_cb(btn, cb_click, LV_EVENT_CLICKED, (void *)(uintptr_t)id);

    lbl = ui_label_text(btn, UI_C_VALUE, text);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);

    return btn;
}
#endif

#if LV_USE_SWITCH
lv_obj_t *ui_switch_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint8_t id)
{
    lv_obj_t *sw = lv_switch_create(parent);

    lv_obj_set_pos(sw, x, y);
    lv_obj_set_style_bg_color(sw, UI_C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, UI_C_ACCENT, LV_PART_INDICATOR | LV_STATE_CHECKED);

    lv_obj_add_event_cb(sw, cb_onoff, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return sw;
}
#endif

#if LV_USE_CHECKBOX
lv_obj_t *ui_checkbox_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          const char *text, uint8_t id)
{
    lv_obj_t *cb = lv_checkbox_create(parent);

    lv_obj_set_pos(cb, x, y);
    lv_checkbox_set_text(cb, (text != NULL) ? text : "");
    lv_obj_set_style_text_font(cb, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cb, UI_C_TEXT, 0);

    lv_obj_add_event_cb(cb, cb_onoff, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return cb;
}
#endif

#if LV_USE_SLIDER
lv_obj_t *ui_slider_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h,
                        int32_t min, int32_t max, int32_t val, uint8_t id)
{
    lv_obj_t *sld = lv_slider_create(parent);

    lv_obj_set_pos(sld, x, y);
    lv_obj_set_size(sld, w, h);

    lv_slider_set_range(sld, min, max);
    lv_slider_set_value(sld, val, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(sld, UI_C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(sld, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(sld, UI_C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(sld, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sld, UI_C_TEXT, LV_PART_KNOB);
    lv_obj_set_style_shadow_width(sld, 0, LV_PART_KNOB);

    /* 只有松手才发一条 —— 拖动中一条都不发 */
    lv_obj_add_event_cb(sld, cb_slider_done, LV_EVENT_RELEASED, (void *)(uintptr_t)id);

    return sld;
}
#endif

#if LV_USE_ARC
lv_obj_t *ui_arc_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                     int32_t min, int32_t max, int32_t val, uint8_t id)
{
    lv_obj_t *arc = lv_arc_create(parent);

    lv_obj_set_pos(arc, x, y);
    lv_obj_set_size(arc, size, size);

    lv_arc_set_bg_angles(arc, 135, 45);
    lv_arc_set_range(arc, min, max);
    lv_arc_set_value(arc, val);

    lv_obj_set_style_arc_color(arc, UI_C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, UI_C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, UI_C_TEXT, LV_PART_KNOB);

    lv_obj_add_event_cb(arc, cb_arc_done, LV_EVENT_RELEASED, (void *)(uintptr_t)id);

    return arc;
}
#endif

#if LV_USE_DROPDOWN
lv_obj_t *ui_dropdown_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, const char *options, uint8_t id)
{
    lv_obj_t *dd = lv_dropdown_create(parent);

    lv_obj_set_pos(dd, x, y);
    lv_obj_set_width(dd, w);
    lv_dropdown_set_options(dd, (options != NULL) ? options : "-");

    lv_obj_set_style_bg_color(dd, UI_C_CARD, 0);
    lv_obj_set_style_text_color(dd, UI_C_TEXT, 0);
    lv_obj_set_style_border_color(dd, UI_C_BORDER, 0);
    lv_obj_set_style_border_width(dd, 1, 0);
    lv_obj_set_style_radius(dd, 10, 0);

    lv_obj_add_event_cb(dd, cb_dropdown, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return dd;
}
#endif

#if LV_USE_ROLLER
lv_obj_t *ui_roller_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, const char *options, uint8_t id)
{
    lv_obj_t *rl = lv_roller_create(parent);

    lv_obj_set_pos(rl, x, y);
    lv_obj_set_size(rl, w, h);
    lv_roller_set_options(rl, (options != NULL) ? options : "-", LV_ROLLER_MODE_NORMAL);

    lv_obj_set_style_bg_color(rl, UI_C_CARD, 0);
    lv_obj_set_style_text_color(rl, UI_C_TEXT, 0);
    lv_obj_set_style_border_color(rl, UI_C_BORDER, 0);
    lv_obj_set_style_border_width(rl, 1, 0);

    lv_obj_add_event_cb(rl, cb_roller, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return rl;
}
#endif

#if LV_USE_BTNMATRIX
lv_obj_t *ui_btnmatrix_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, const char **map, uint8_t id)
{
    lv_obj_t *bm = lv_btnmatrix_create(parent);

    lv_obj_set_pos(bm, x, y);
    lv_obj_set_size(bm, w, h);
    lv_btnmatrix_set_map(bm, map);

    lv_obj_set_style_bg_color(bm, UI_C_CARD, 0);
    lv_obj_set_style_border_color(bm, UI_C_BORDER, 0);
    lv_obj_set_style_border_width(bm, 1, 0);

    lv_obj_add_event_cb(bm, cb_btnmatrix, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return bm;
}
#endif

#if LV_USE_TEXTAREA
lv_obj_t *ui_textarea_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, const char *placeholder, uint8_t id)
{
    lv_obj_t *ta = lv_textarea_create(parent);

    lv_obj_set_pos(ta, x, y);
    lv_obj_set_size(ta, w, h);

    if(placeholder != NULL)
    {
        lv_textarea_set_placeholder_text(ta, placeholder);
    }

    lv_obj_set_style_bg_color(ta, UI_C_CARD, 0);
    lv_obj_set_style_text_color(ta, UI_C_TEXT, 0);
    lv_obj_set_style_border_color(ta, UI_C_BORDER, 0);
    lv_obj_set_style_border_width(ta, 1, 0);

    lv_obj_add_event_cb(ta, cb_ready,  LV_EVENT_READY,  (void *)(uintptr_t)id);
    lv_obj_add_event_cb(ta, cb_cancel, LV_EVENT_CANCEL, (void *)(uintptr_t)id);

    return ta;
}
#endif

#if LV_USE_KEYBOARD
lv_obj_t *ui_keyboard_new(lv_obj_t *parent, lv_coord_t w, uint8_t id)
{
    lv_obj_t *kb = lv_keyboard_create(parent);

    lv_obj_set_width(kb, w);

    lv_obj_add_event_cb(kb, cb_ready,  LV_EVENT_READY,  (void *)(uintptr_t)id);
    lv_obj_add_event_cb(kb, cb_cancel, LV_EVENT_CANCEL, (void *)(uintptr_t)id);

    return kb;
}
#endif

#if LV_USE_CALENDAR
lv_obj_t *ui_calendar_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, uint8_t id)
{
    lv_obj_t *cal = lv_calendar_create(parent);

    lv_obj_set_pos(cal, x, y);
    lv_obj_set_size(cal, w, h);

    lv_obj_set_style_bg_color(cal, UI_C_CARD, 0);
    lv_obj_set_style_border_color(cal, UI_C_BORDER, 0);
    lv_obj_set_style_border_width(cal, 1, 0);

    lv_obj_add_event_cb(cal, cb_calendar, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    return cal;
}
#endif

/**************************************************************************************************
 *                                        只显示、不交互
 **************************************************************************************************/
#if LV_USE_BAR
lv_obj_t *ui_bar_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, int32_t min, int32_t max)
{
    lv_obj_t *bar = lv_bar_create(parent);

    lv_obj_set_pos(bar, x, y);
    lv_obj_set_size(bar, w, h);

    lv_bar_set_range(bar, min, max);
    lv_bar_set_value(bar, min, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(bar, UI_C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, UI_C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, h / 2, LV_PART_INDICATOR);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_CLICKABLE);

    return bar;
}

void ui_bar_set(lv_obj_t *bar, int32_t val)
{
    if(bar != NULL)
    {
        lv_bar_set_value(bar, val, LV_ANIM_OFF);
    }
}
#endif

#if LV_USE_LED
lv_obj_t *ui_led_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t size, lv_color_t color)
{
    lv_obj_t *led = lv_led_create(parent);

    lv_obj_set_pos(led, x, y);
    lv_obj_set_size(led, size, size);
    lv_led_set_color(led, color);
    lv_led_set_brightness(led, LV_LED_BRIGHT_MAX);

    return led;
}

void ui_led_set(lv_obj_t *led, uint8_t on)
{
    if(led == NULL)
    {
        return;
    }

    lv_led_set_brightness(led, (on != 0) ? LV_LED_BRIGHT_MAX : LV_LED_BRIGHT_MIN);
}
#endif

#if LV_USE_SPINNER
lv_obj_t *ui_spinner_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t size, uint16_t time_ms)
{
    lv_obj_t *sp = lv_spinner_create(parent, time_ms, 60);

    lv_obj_set_pos(sp, x, y);
    lv_obj_set_size(sp, size, size);

    lv_obj_set_style_arc_color(sp, UI_C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(sp, UI_C_ACCENT, LV_PART_INDICATOR);

    return sp;
}
#endif

#if LV_USE_LINE
lv_obj_t *ui_line_new(lv_obj_t *parent, const lv_point_t *points, uint16_t cnt, lv_color_t color)
{
    lv_obj_t *line = lv_line_create(parent);

    lv_line_set_points(line, points, cnt);
    lv_obj_set_style_line_color(line, color, 0);
    lv_obj_set_style_line_width(line, 2, 0);

    return line;
}
#endif

#if LV_USE_IMG
lv_obj_t *ui_img_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const void *src)
{
    lv_obj_t *img = lv_img_create(parent);

    lv_obj_set_pos(img, x, y);
    lv_img_set_src(img, src);

    return img;
}
#endif

#if LV_USE_TABLE
lv_obj_t *ui_table_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       lv_coord_t w, lv_coord_t h, uint16_t rows, uint16_t cols)
{
    lv_obj_t *tbl = lv_table_create(parent);

    lv_obj_set_pos(tbl, x, y);
    lv_obj_set_size(tbl, w, h);
    lv_table_set_row_cnt(tbl, rows);
    lv_table_set_col_cnt(tbl, cols);

    lv_obj_set_style_bg_color(tbl, UI_C_CARD, 0);
    lv_obj_set_style_text_color(tbl, UI_C_TEXT, 0);
    lv_obj_set_style_border_color(tbl, UI_C_BORDER, 0);

    return tbl;
}
#endif

#if LV_USE_CHART
lv_obj_t *ui_chart_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       lv_coord_t w, lv_coord_t h, int32_t max)
{
    lv_obj_t *chart = lv_chart_create(parent);

    lv_obj_set_pos(chart, x, y);
    lv_obj_set_size(chart, w, h);
    lv_chart_set_type(chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(chart, w);
    lv_chart_set_range(chart, LV_CHART_AXIS_PRIMARY_Y, 0, max);
    lv_chart_add_series(chart, UI_C_ACCENT, LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_set_style_bg_color(chart, UI_C_CARD, 0);
    lv_obj_set_style_border_color(chart, UI_C_BORDER, 0);
    lv_obj_set_style_line_width(chart, 2, LV_PART_ITEMS);

    return chart;
}
#endif

#if LV_USE_METER
lv_obj_t *ui_meter_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                       int32_t min, int32_t max, int32_t val)
{
    lv_obj_t *meter = lv_meter_create(parent);
    lv_meter_scale_t *scale;
    lv_meter_indicator_t *needle;

    lv_obj_set_pos(meter, x, y);
    lv_obj_set_size(meter, size, size);
    lv_obj_remove_style_all(meter);
    lv_obj_set_style_bg_opa(meter, LV_OPA_TRANSP, 0);

    scale = lv_meter_add_scale(meter);
    lv_meter_set_scale_ticks(meter, scale, 21, 2, 8, UI_C_BORDER);
    lv_meter_set_scale_range(meter, scale, min, max, 270, 135);

    needle = lv_meter_add_needle_line(meter, scale, 3, UI_C_ACCENT, -8);
    lv_meter_set_indicator_value(meter, needle, val);

    return meter;
}
#endif

#if LV_USE_CANVAS
lv_obj_t *ui_canvas_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, lv_color_t *buf)
{
    lv_obj_t *cv = lv_canvas_create(parent);

    lv_obj_set_pos(cv, x, y);
    lv_obj_set_size(cv, w, h);
    lv_canvas_set_buffer(cv, buf, w, h, LV_IMG_CF_TRUE_COLOR);

    return cv;
}
#endif
