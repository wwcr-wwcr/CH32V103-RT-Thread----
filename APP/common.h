/******************************************************************************
 * 文件名   : common.h
 * 说明     : LVGL 控件封装 —— 一套统一风格的"造控件"函数，界面文件只用这个
 *
 * 为什么要有它：
 *     每个界面都在重复写"圆角 + 配色 + 边框 + 内边距 + 点一下发事件"这十几行，
 *     改成调一个函数，界面文件就只剩版式（谁放哪、什么文字）。
 *
 * 约定：
 *     1. 所有函数名 ui_xxx_new()，参数顺序统一为 (parent, x, y, w/h..., 文字, id)。
 *     2. 交互类控件最后那个 id 是"控件编号"，点了会 kernel_post(TOUCH, id, 值)：
 *          按钮 / 复选框 / 按钮矩阵   → value = 0
 *          开关                      → value = 1 / 0
 *          滑条 / 圆弧 / 下拉框 / 滚轮 → value = 当前值（滑条和圆弧只在松手时发）
 *     3. 只画不交互的控件没有 id 参数，返回句柄给界面自己更新。
 *     4. 未启用的控件（lv_conf.h 里 LV_USE_XXX = 0）整段被 #if 包掉，
 *        等你打开对应宏，函数自动就有，不用改这里。
 *     5. 坐标都是"父对象内部坐标"，左上角为原点，单位像素（屏幕 320×480）。
 *
 * 没有封装的控件及原因（真要用时照样式补一个）：
 *     msgbox / list / menu / tabview / tileview / win
 *         都是"组合控件"，由 obj + label + btn 拼出来，封装反而限制版式
 *     span / spinbox
 *         依赖 textarea，等真用到再包
 *     animimg / imgbtn / colorwheel
 *         需要图片或动画帧数据，没法给出通用默认样式
 ******************************************************************************/

#ifndef __COMMON_H
#define __COMMON_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================ 基础：页面 / 卡片 / 文字 ============================ */

/**
 * @brief  造一屏（带渐变底色的整屏对象）
 * @return 屏幕对象；不使用时不要删，交给页面销毁逻辑统一删
 */
lv_obj_t *ui_page_new(void);

/**
 * @brief  造一张圆角卡片（容器用，不响应点击）
 * @param  parent  父对象，一般是 ui_page_new() 或另一张卡片
 * @param  x       左边缘 X 坐标（父对象内部坐标）
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度
 * @param  h       高度
 * @param  radius  圆角半径，0 = 直角；常用 14~18
 * @return 卡片对象（往里放子控件时把它当 parent）
 */
lv_obj_t *ui_card_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                      lv_coord_t w, lv_coord_t h, lv_coord_t radius);

/**
 * @brief  造一段文字（透明底、无边框，字体固定 montserrat_14）
 * @param  parent  父对象
 * @param  color   文字颜色，取 theme.h 里的 UI_C_* 宏
 * @return 标签对象（之后要改文字用 ui_label_set）
 */
lv_obj_t *ui_label_new(lv_obj_t *parent, lv_color_t color);

/**
 * @brief  造一段带初始文字的文字
 * @param  parent  父对象
 * @param  color   文字颜色
 * @param  text    初始文字，只支持 ASCII（没有中文字库）；传 NULL 当空串
 * @return 标签对象
 */
lv_obj_t *ui_label_text(lv_obj_t *parent, lv_color_t color, const char *text);

/**
 * @brief  改文字内容
 * @param  lbl   标签对象；传 NULL 安全返回（省掉调用方的判空）
 * @param  text  新文字，传 NULL 当空串
 */
void      ui_label_set(lv_obj_t *lbl, const char *text);

/* ============================ 交互类（点了自动发 TOUCH 事件） ============================ */

#if LV_USE_BTN
/**
 * @brief  造一个按钮（圆角卡片样式 + 居中文字），点一下发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度
 * @param  h       高度
 * @param  text    按钮文字（ASCII），NULL 当空串
 * @param  id      控件编号 0~255，点了 kernel_post(TOUCH, id, 0)
 * @return 按钮对象（要单独改样式/禁用时用）
 */
lv_obj_t *ui_btn_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, const char *text, uint8_t id);
#endif

#if LV_USE_SWITCH
/**
 * @brief  造一个开关，拨动时发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  id      控件编号，kernel_post(TOUCH, id, 1 开 / 0 关)
 * @return 开关对象
 */
lv_obj_t *ui_switch_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, uint8_t id);
#endif

#if LV_USE_CHECKBOX
/**
 * @brief  造一个复选框（文字在右边），勾选变化时发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  text    复选框文字（ASCII），NULL 当空串
 * @param  id      控件编号，kernel_post(TOUCH, id, 1 选中 / 0 取消)
 * @return 复选框对象
 */
lv_obj_t *ui_checkbox_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          const char *text, uint8_t id);
#endif

#if LV_USE_SLIDER
/**
 * @brief  造一条滑条；拖动过程不发消息，松手才发一条
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度（建议 >= 100，太短不好点）
 * @param  h       高度（轨道粗细，常用 8~12）
 * @param  min     最小值（左端）
 * @param  max     最大值（右端），建议直接对齐业务单位，省得换算
 * @param  val     初始值，必须在 min~max 之间
 * @param  id      控件编号，松手时 kernel_post(TOUCH, id, 当前值)
 * @return 滑条对象
 * @note   拖动过程中想要实时反馈（比如数字跟着变），自己再 add 一个
 *         LV_EVENT_VALUE_CHANGED 回调，里面只改界面、不要 kernel_post
 */
lv_obj_t *ui_slider_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h,
                        int32_t min, int32_t max, int32_t val, uint8_t id);
#endif

#if LV_USE_ARC
/**
 * @brief  造一个圆弧（转盘），松手时发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  size    直径（宽 = 高 = size）
 * @param  min     最小角度对应的值
 * @param  max     最大角度对应的值
 * @param  val     初始值，必须在 min~max 之间
 * @param  id      控件编号，松手时 kernel_post(TOUCH, id, 当前值)
 * @return 圆弧对象
 */
lv_obj_t *ui_arc_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                     int32_t min, int32_t max, int32_t val, uint8_t id);
#endif

#if LV_USE_DROPDOWN
/**
 * @brief  造一个下拉框，选择变化时发 TOUCH 事件
 * @param  parent   父对象
 * @param  x        左边缘 X 坐标
 * @param  y        上边缘 Y 坐标
 * @param  w        宽度（高度由主题决定，通常 40 左右）
 * @param  options  选项表，用 '\n' 分隔，例如 "AUTO\nMANUAL\nOFF"
 * @param  id       控件编号，kernel_post(TOUCH, id, 选中项下标 0 开始)
 * @return 下拉框对象
 */
lv_obj_t *ui_dropdown_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, const char *options, uint8_t id);
#endif

#if LV_USE_ROLLER
/**
 * @brief  造一个滚轮选择器，选择变化时发 TOUCH 事件
 * @param  parent   父对象
 * @param  x        左边缘 X 坐标
 * @param  y        上边缘 Y 坐标
 * @param  w        宽度
 * @param  h        高度（要能露出 3~5 行，常用 100~120）
 * @param  options  选项表，用 '\n' 分隔
 * @param  id       控件编号，kernel_post(TOUCH, id, 选中项下标 0 开始)
 * @return 滚轮对象
 */
lv_obj_t *ui_roller_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, const char *options, uint8_t id);
#endif

#if LV_USE_BTNMATRIX
/**
 * @brief  造一个按钮矩阵（一排/多排小按钮），按下时发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       总宽度
 * @param  h       总高度
 * @param  map     按钮文字表，一行一个字符串，最后必须是 "" 结尾
 *                 例如 {"1","2","3","","4","5","6",""} 表示两行
 * @param  id      控件编号，kernel_post(TOUCH, id, 按下第几个 0 开始)
 * @return 矩阵对象
 */
lv_obj_t *ui_btnmatrix_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, const char **map, uint8_t id);
#endif

#if LV_USE_TEXTAREA
/**
 * @brief  造一个单行输入框（配 ui_keyboard_new 用），确认/取消时发事件
 * @param  parent       父对象
 * @param  x            左边缘 X 坐标
 * @param  y            上边缘 Y 坐标
 * @param  w            宽度
 * @param  h            高度（单行常用 44）
 * @param  placeholder  未输入时显示的灰色提示，NULL 不显示
 * @param  id           控件编号：确认 kernel_post(TOUCH, id, 1)，取消发 0
 * @return 输入框对象（取内容用 lv_textarea_get_text）
 */
lv_obj_t *ui_textarea_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, const char *placeholder, uint8_t id);
#endif

#if LV_USE_KEYBOARD
/**
 * @brief  造一个屏幕键盘（一般放屏幕底部），确认/取消时发事件
 * @param  parent  父对象
 * @param  w       宽度（通常给满屏宽 UI_W）
 * @param  id      控件编号：确认 kernel_post(TOUCH, id, 1)，取消发 0
 * @return 键盘对象；要绑输入框用 lv_keyboard_set_textarea()
 */
lv_obj_t *ui_keyboard_new(lv_obj_t *parent, lv_coord_t w, uint8_t id);
#endif

#if LV_USE_CALENDAR
/**
 * @brief  造一个日历，点日期时发 TOUCH 事件
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度（建议 >= 240）
 * @param  h       高度（建议 >= 220）
 * @param  id      控件编号，kernel_post(TOUCH, id, 点中的"日" 1~31)
 * @return 日历对象
 */
lv_obj_t *ui_calendar_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                          lv_coord_t w, lv_coord_t h, uint8_t id);
#endif

/* ============================ 只显示、不交互 ============================ */

#if LV_USE_BAR
/**
 * @brief  造一条进度条（不响应点击）
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度
 * @param  h       高度（常用 8~12，圆角自动取 h/2）
 * @param  min     最小值（空条）
 * @param  max     最大值（满条）
 * @return 进度条对象；之后用 ui_bar_set 更新
 */
lv_obj_t *ui_bar_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t w, lv_coord_t h, int32_t min, int32_t max);

/**
 * @brief  更新进度条的值（无动画）
 * @param  bar  进度条对象；传 NULL 安全返回
 * @param  val  新值，会自动夹到 min~max
 */
void      ui_bar_set(lv_obj_t *bar, int32_t val);
#endif

#if LV_USE_LED
/**
 * @brief  造一个圆形指示灯（创建时默认点亮）
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  size    直径（宽 = 高 = size）
 * @param  color   灯的颜色，取 theme.h 里的宏
 * @return 指示灯对象；之后用 ui_led_set 开关
 */
lv_obj_t *ui_led_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                     lv_coord_t size, lv_color_t color);

/**
 * @brief  点亮 / 熄灭指示灯（熄灭时变暗，不是消失）
 * @param  led  指示灯对象；传 NULL 安全返回
 * @param  on   1 = 最亮，0 = 最暗
 */
void      ui_led_set(lv_obj_t *led, uint8_t on);
#endif

#if LV_USE_SPINNER
/**
 * @brief  造一个转圈动画（忙等提示）
 * @param  parent   父对象
 * @param  x        左边缘 X 坐标
 * @param  y        上边缘 Y 坐标
 * @param  size     直径
 * @param  time_ms  转一圈的毫秒数，常用 800~1500
 * @return 转圈对象
 */
lv_obj_t *ui_spinner_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                         lv_coord_t size, uint16_t time_ms);
#endif

#if LV_USE_LINE
/**
 * @brief  造一条折线（用点集画，做简单曲线/示波器用）
 * @param  parent  父对象
 * @param  points  点数组，坐标是"父对象内部坐标"
 * @param  cnt     点个数
 * @param  color   线颜色
 * @return 折线对象
 * @note   points 必须一直有效（别传局部数组），LVGL 只存指针不拷贝
 */
lv_obj_t *ui_line_new(lv_obj_t *parent, const lv_point_t *points, uint16_t cnt, lv_color_t color);
#endif

#if LV_USE_IMG
/**
 * @brief  贴一张图片
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  src     图片来源：LV_IMG_DECLARE 出来的变量名（传 &xxx）或文件路径字符串
 * @return 图片对象
 * @note   需要 lv_conf.h 里 LV_USE_IMG = 1，并且工程里真有图片资源
 */
lv_obj_t *ui_img_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, const void *src);
#endif

#if LV_USE_TABLE
/**
 * @brief  造一个表格（数据自己用 lv_table_set_cell_value 填）
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       总宽
 * @param  h       总高
 * @param  rows    行数（不含表头，表头自己占第 0 行）
 * @param  cols    列数
 * @return 表格对象
 */
lv_obj_t *ui_table_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       lv_coord_t w, lv_coord_t h, uint16_t rows, uint16_t cols);
#endif

#if LV_USE_CHART
/**
 * @brief  造一张折线图（0~max 的纵轴，点数 = 宽度像素）
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽度（同时决定保留多少个点，1 列 1 点）
 * @param  h       高度
 * @param  max     纵轴最大值，数据超过会被夹到顶
 * @return 图表对象；加数据用 lv_chart_set_next_value(chart, 系列, 值)
 * @note   要拿系列指针：lv_chart_get_series_next(chart, NULL)
 */
lv_obj_t *ui_chart_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                       lv_coord_t w, lv_coord_t h, int32_t max);
#endif

#if LV_USE_METER
/**
 * @brief  造一个指针式仪表盘
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  size    直径
 * @param  min     刻度最小值（表盘左端）
 * @param  max     刻度最大值（表盘右端）
 * @param  val     指针初始值，必须在 min~max 之间
 * @return 仪表对象；改指针用 lv_meter_set_indicator_value()
 */
lv_obj_t *ui_meter_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t size,
                       int32_t min, int32_t max, int32_t val);
#endif

#if LV_USE_CANVAS
/**
 * @brief  造一块画布（可以直接往像素缓冲里写，做波形/位图用）
 * @param  parent  父对象
 * @param  x       左边缘 X 坐标
 * @param  y       上边缘 Y 坐标
 * @param  w       宽（像素）
 * @param  h       高（像素）
 * @param  buf     像素缓冲，大小 w*h*2 字节（RGB565），必须一直有效
 * @return 画布对象；画点用 lv_canvas_set_px()，整块刷用 lv_canvas_flush 系列
 * @note   LV_USE_CANVAS 打开后 LV_MEM_SIZE 的压力会变大，先算好缓冲再开
 */
lv_obj_t *ui_canvas_new(lv_obj_t *parent, lv_coord_t x, lv_coord_t y,
                        lv_coord_t w, lv_coord_t h, lv_color_t *buf);
#endif

#ifdef __cplusplus
}
#endif

#endif
