/******************************************************************************
 * 文件名   : demo_chart.h
 * 说明     : 图表 demo 合集（移植自 CH32H417 的 LVGL_Demo/Chart_Demo）
 *
 * 四种形态，一个函数切换：
 *     OSC      示波器：256 点，数据左移、新点在右边进来
 *     SPECTRUM 频谱：64 根柱子，0~100，适合 FFT 结果
 *     COORD    坐标系：带 Y 轴刻度和网格，看整体趋势
 *     SINGLE   单曲线：128 点，整帧替换
 *
 * 用法（在 task 的 render 里）：
 *     demo_chart_create(s_page, DEMO_CHART_OSC);   // 建一次
 *     demo_chart_reflash();                        // 每 30ms 推一次数据
 *     demo_chart_destroy();                        // 删页前调用
 ******************************************************************************/

#ifndef __DEMO_CHART_H
#define __DEMO_CHART_H

#include "lvgl/lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    DEMO_CHART_OSC = 0,     /* 示波器：滚动 */
    DEMO_CHART_SPECTRUM,    /* 频谱柱 */
    DEMO_CHART_COORD,       /* 坐标系 */
    DEMO_CHART_SINGLE,      /* 单曲线 */
    DEMO_CHART_NUM
} demo_chart_t;

/**
 * @brief  名称（显示在标题栏），如 "OSC" / "SPECTRUM"
 */
const char *demo_chart_name(demo_chart_t kind);

/**
 * @brief  在页面里建图表（会先删掉上一次建的）
 * @param  page  页面对象（ui_page_new 的返回值）
 * @param  kind  形态
 */
void demo_chart_create(lv_obj_t *page, demo_chart_t kind);

/**
 * @brief  推一次演示数据（自己造波形，不依赖真实采样）
 */
void demo_chart_reflash(void);

/**
 * @brief  删掉图表对象（删页前调用；不调也不会漏，但句柄要先置空）
 */
void demo_chart_destroy(void);

#ifdef __cplusplus
}
#endif

#endif
