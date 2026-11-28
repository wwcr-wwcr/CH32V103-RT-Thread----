/**
 * @file lv_conf.h
 * @brief LVGL 8.2 配置（只覆盖本工程真正需要的项，其余全部走 lv_conf_internal.h 默认值）
 *
 * 原则：
 *   1. 不关任何控件开关。控件之间的依赖很多（spinbox 依赖 textarea、keyboard 依赖
 *      btnmatrix），手动关错一个就编译不过；没用到的控件由 --gc-sections 在链接时丢掉，
 *      不会占 Flash。
 *   2. 只在必须改的地方写 #define：颜色深度、内存池、时基、字体、调试。
 *   3. 注释里不要出现注释结束符，避免把后面的代码吃掉（之前就栽在这）。
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* 只做前置声明。在这里 include rtthread.h 会把整套 RT-Thread 头文件
 * 拉进每一个 LVGL 源文件，没必要。 */
extern void rt_kprintf(const char *fmt, ...);

/*====================
   颜色
 *====================*/
#define LV_COLOR_DEPTH          16          /* RGB565，和 ST7796S 的 16 位并口一致 */
#define LV_COLOR_16_SWAP        0

/*====================
   内存（LVGL 自己的堆，不是 RT-Thread 的）
 *====================*/
#define LV_MEM_CUSTOM           0           /* 用 LVGL 内置 TLSF */
#define LV_MEM_SIZE             (32U * 1024U)
#define LV_GRAD_CACHE_DEF_SIZE  (4U * 1024U)   /* gradient cache: 4KB, must fit inside LV_MEM_SIZE */

/*====================
   时基：直接用 RT-Thread 的 tick（1ms）
 *====================*/
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "rtthread.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (rt_tick_get())

/*====================
   刷新 / 输入周期
 *====================*/
#define LV_DISP_DEF_REFR_PERIOD 30
#define LV_INDEV_DEF_READ_PERIOD 30

/*====================
   字体：只开 14 号
 *====================*/
#define LV_FONT_MONTSERRAT_14   1
/* 中文字库做默认，缺的字（英文/数字/符号）自动回退到 montserrat_14 */
#define LV_FONT_CUSTOM_DECLARE  LV_FONT_DECLARE(lv_font_cn12) LV_FONT_DECLARE(lv_font_cn16)
#define LV_FONT_DEFAULT         &lv_font_cn12

/*====================
   控件开关
   保留：label / bar / line / slider / btn（界面全用这几个）
   其余全关。注意依赖关系：keyboard 需要 btnmatrix、spinbox 需要 textarea、
   calendar/msgbox/tabview 需要 btnmatrix —— 关的时候成对关，否则编译不过。
 *====================*/
#define LV_USE_ARC              0   // 弧形进度条/圆弧控件
#define LV_USE_ANIMIMG          0   // 动画图像控件
#define LV_USE_BTNMATRIX        0   // 按钮矩阵控件
#define LV_USE_CALENDAR         0   // 日历控件
#define LV_USE_CANVAS           0   // 画布控件（可绘图）
#define LV_USE_CHART            1   // 图表控件
#define LV_USE_CHECKBOX         0   // 复选框控件
#define LV_USE_COLORWHEEL       0   // 色轮控件
#define LV_USE_DROPDOWN         0   // 下拉列表控件
#define LV_USE_IMG              0   // 图像控件
#define LV_USE_BTN              1   // 按钮控件（卡片都用 lv_obj_create 做，不需要）
#define LV_USE_BAR              1   // 进度条控件（没用）
#define LV_USE_LINE             1   // 线条控件（没用）
#define LV_USE_SLIDER           1   // 滑条控件（没用）
#define LV_USE_IMGBTN           0   // 图形按钮控件（LV_USE_IMG 已关，没意义）
#define LV_USE_KEYBOARD         0   // 屏幕键盘控件
#define LV_USE_LED              0   // LED 指示灯控件
#define LV_USE_LIST             0   // 列表控件
#define LV_USE_MENU             0   // 菜单控件
#define LV_USE_METER            0   // 仪表盘控件
#define LV_USE_MSGBOX           0   // 消息框控件
#define LV_USE_ROLLER           0   // 滚轮选择器控件
#define LV_USE_SPAN             0   // 富文本/多风格文本片段控件
#define LV_USE_SPINBOX          0   // 数字微调框控件
#define LV_USE_SPINNER          0   // 加载旋转器控件
#define LV_USE_SWITCH           0   // 开关控件
#define LV_USE_TABLE            0   // 表格控件
#define LV_USE_TABVIEW          0   // 选项卡视图控件
#define LV_USE_TEXTAREA         0   // 文本输入区域控件
#define LV_USE_TILEVIEW         0   // 平铺视图控件
#define LV_USE_WIN              0   // 窗口控件
#define LV_USE_THEME_BASIC      0   // 基础主题
#define LV_USE_THEME_MONO       0   // 单色主题

/*====================
   布局：只用 flex（本项目实际没用到，留着不影响）
 *====================*/#define LV_USE_GRID             0

/*====================
   调试
 *====================*/
#define LV_USE_LOG              0
#define LV_USE_ASSERT_NULL      1
#define LV_USE_ASSERT_MALLOC    1
#define LV_USE_PERF_MONITOR     0
#define LV_USE_MEM_MONITOR      1

/*====================
   断言处理
   默认是静默的 while(1);，LVGL 堆耗尽时串口一个字都不吐，
   现象和"死机"完全一样。改成先打印位置再停机。
 *====================*/
#ifndef LV_ASSERT_HANDLER
/* LVGL 把这个宏展开在右花括号之前、且不带分号，所以宏体末尾必须自己带 */
#define LV_ASSERT_HANDLER                                                     \
    do {                                                                      \
        rt_kprintf("\r\n[LVGL ASSERT] %s:%d\r\n", __FILE__, __LINE__);        \
        while(1) { }                                                          \
    } while(0);
#endif

#endif /*LV_CONF_H*/