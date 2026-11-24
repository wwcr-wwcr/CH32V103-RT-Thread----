/******************************************************************************
 * 文件名   : demo_chart.c
 * 说明     : 四种图表形态的实现
 *
 * 移植自 CH32H417 模板的 Chart_Demo（Oscilloscope / Spectrum / Coordinate / Single），
 * 但改成用本工程的 ui_* 工厂 + theme 配色，且不依赖中文字体（只用默认字体，文字全 ASCII）。
 *
 * 关键技术（模板里最值得抄的两条）：
 *   1. lv_chart_set_ext_y_array —— 图表直接指向我们的数组，零拷贝
 *   2. 自己 memmove 做滚动 —— 效果等同 LV_CHART_UPDATE_MODE_SHIFT，但可控
 ******************************************************************************/

#include "demo_chart.h"
#include "theme.h"
#include "LVGL_DEMO/LVGL_Demo.h"
#include <string.h>

/**************************************************************************************************
 *                                        常量与状态
 **************************************************************************************************/
#define OSC_N       256U        /* 示波器点数 */
#define SPEC_N      64U         /* 频谱柱数 */
#define COORD_N     128U        /* 坐标系点数 */
#define SINGLE_N    128U        /* 单曲线点数 */

#define DATA_MAX    OSC_N       /* 缓冲按最大点数开（四种共用） */

static lv_obj_t          *s_chart = NULL;
static lv_chart_series_t *s_ser   = NULL;
static demo_chart_t       s_kind  = DEMO_CHART_OSC;

static int16_t  s_buf[DATA_MAX];        /* 图表数据源（ext 数组，必须常驻） */
static uint16_t s_phase = 0;            /* 造波形用相位 0..1023 */
static uint32_t s_rand  = 0x1234567U;   /* 伪随机，造噪声 */

/**************************************************************************************************
 *                                        小工具
 **************************************************************************************************/
/* 三角波：输入 0..1023，输出 0..1000（不用 libm） */
static uint32_t tri(uint16_t ph)
{
    ph &= 1023U;

    return (ph < 512U) ? ((uint32_t)ph * 1000U / 512U)
                       : ((uint32_t)(1024U - ph) * 1000U / 512U);
}

/* xorshift 伪随机 */
static uint32_t rnd(void)
{
    s_rand ^= s_rand << 13;
    s_rand ^= s_rand >> 17;
    s_rand ^= s_rand << 5;

    return s_rand;
}

/**************************************************************************************************
 *                                        对外接口
 **************************************************************************************************/
const char *demo_chart_name(demo_chart_t kind)
{
    switch(kind)
    {
        case DEMO_CHART_OSC:      return "OSC";
        case DEMO_CHART_SPECTRUM: return "SPECTRUM";
        case DEMO_CHART_COORD:    return "COORD";
        case DEMO_CHART_SINGLE:   return "SINGLE";
        default:                  return "?";
    }
}

void demo_chart_destroy(void)
{
    if(s_chart != NULL)
    {
        lv_obj_del(s_chart);
        s_chart = NULL;
        s_ser   = NULL;
    }
}

void demo_chart_create(lv_obj_t *page, demo_chart_t kind)
{
    demo_chart_destroy();

    if(page == NULL)
    {
        return;
    }

    s_kind = kind;

    switch(kind)
    {
        case DEMO_CHART_OSC:
            /* 示波器：256 点，0~4095，左侧 4 格刻度 */
            s_chart = ui_chart_new(page, 12, 80, UI_W - 24, 200, 4095);
            lv_chart_set_point_count(s_chart, OSC_N);
            lv_chart_set_div_line_count(s_chart, 4, 8);
            lv_chart_set_axis_tick(s_chart, LV_CHART_AXIS_PRIMARY_Y, 4, 2, 4, 1, true, 34);
            break;

        case DEMO_CHART_SPECTRUM:
            /* 频谱：64 根柱子，0~100 */
            s_chart = ui_chart_new(page, 12, 80, UI_W - 24, 200, 100);
            lv_chart_set_type(s_chart, LV_CHART_TYPE_BAR);
            lv_chart_set_point_count(s_chart, SPEC_N);
            lv_chart_set_div_line_count(s_chart, 5, 1);
            break;

        case DEMO_CHART_COORD:
            /* 坐标系：128 点，带刻度网格 */
            s_chart = ui_chart_new(page, 12, 80, UI_W - 24, 200, 4095);
            lv_chart_set_point_count(s_chart, COORD_N);
            lv_chart_set_div_line_count(s_chart, 6, 10);
            lv_chart_set_axis_tick(s_chart, LV_CHART_AXIS_PRIMARY_Y, 4, 2, 5, 1, true, 34);
            break;

        case DEMO_CHART_SINGLE:
        default:
            /* 单曲线：128 点，0~1000 */
            s_chart = ui_chart_new(page, 12, 80, UI_W - 24, 200, 1000);
            lv_chart_set_point_count(s_chart, SINGLE_N);
            lv_chart_set_div_line_count(s_chart, 4, 6);
            break;
    }

    /* 把数据源绑成外部数组（四种形态共用 s_buf） */
    s_ser = lv_chart_get_series_next(s_chart, NULL);
    if(s_ser != NULL)
    {
        lv_chart_set_ext_y_array(s_chart, s_ser, s_buf);
    }

    memset(s_buf, 0, sizeof(s_buf));
    demo_chart_reflash();
}

void demo_chart_reflash(void)
{
    uint16_t i;

    if((s_chart == NULL) || (s_ser == NULL))
    {
        return;
    }

    switch(s_kind)
    {
        case DEMO_CHART_OSC:
        {
            /* 左移 4 点，右边补 4 个新样点 —— 看起来就是波形在滚动 */
            memmove(&s_buf[0], &s_buf[4], (OSC_N - 4U) * sizeof(s_buf[0]));

            for(i = (uint16_t)(OSC_N - 4U); i < OSC_N; i++)
            {
                s_phase = (uint16_t)((s_phase + 9U) & 1023U);
                s_buf[i] = (int16_t)(150U + (tri(s_phase) * 3600U) / 1000U + (rnd() % 60U));
            }
            break;
        }

        case DEMO_CHART_SPECTRUM:
        {
            /* 1/f 底噪 + 三个固定峰，模拟 FFT 输出的样子 */
            for(i = 0; i < SPEC_N; i++)
            {
                uint32_t base = 80U / (1U + i);
                uint32_t peak = ((i == 4U) || (i == 11U) || (i == 22U)) ? 55U : 0U;

                s_buf[i] = (int16_t)(base + peak + (rnd() % 8U));
            }
            break;
        }

        case DEMO_CHART_COORD:
        {
            /* 整帧重画一条缓慢漂移的三角波 */
            s_phase = (uint16_t)((s_phase + 5U) & 1023U);

            for(i = 0; i < COORD_N; i++)
            {
                uint32_t ph = (uint32_t)(s_phase + i * 8U) & 1023U;

                s_buf[i] = (int16_t)(200U + (tri((uint16_t)ph) * 3600U) / 1000U);
            }
            break;
        }

        case DEMO_CHART_SINGLE:
        default:
        {
            /* 整帧替换，幅度慢慢变化 */
            s_phase = (uint16_t)((s_phase + 3U) & 1023U);

            for(i = 0; i < SINGLE_N; i++)
            {
                uint32_t ph = (uint32_t)(s_phase + i * 8U) & 1023U;

                s_buf[i] = (int16_t)((tri((uint16_t)ph) * 900U) / 1000U);
            }
            break;
        }
    }

    lv_chart_refresh(s_chart);      /* ext 数组改完了，通知 LVGL 重画 */
}
