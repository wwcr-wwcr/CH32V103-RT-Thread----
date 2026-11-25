/******************************************************************************
 * 文件名   : vref.c
 * 说明     : 板级电压标定实现 + msh 命令
 ******************************************************************************/

#include "vref.h"
#include "dac.h"
#include "adc.h"
#include "wave.h"
#include "ch32v30x_conf.h"
#include "rtthread.h"
#include <stdlib.h>

static uint16_t s_vdda_mv = VREF_VDDA_MV_DEFAULT;

uint16_t vref_vdda_mv(void)
{
    return s_vdda_mv;
}

void vref_set_vdda_mv(uint16_t mv)
{
    if(mv == 0U)
    {
        s_vdda_mv = VREF_VDDA_MV_DEFAULT;
        return;
    }

    if(mv < VREF_VDDA_MV_MIN) { mv = VREF_VDDA_MV_MIN; }
    if(mv > VREF_VDDA_MV_MAX) { mv = VREF_VDDA_MV_MAX; }

    s_vdda_mv = mv;
}

uint16_t vref_code_to_mv(uint32_t code)
{
    if(code > 4095U)
    {
        code = 4095U;
    }

    return (uint16_t)((code * s_vdda_mv + 2047U) / 4095U);
}

uint16_t vref_mv_to_code(uint32_t mv)
{
    uint32_t code;

    if(mv >= s_vdda_mv)
    {
        return 4095U;
    }

    code = (mv * 4095U + (s_vdda_mv / 2U)) / s_vdda_mv;

    return (uint16_t)((code > 4095U) ? 4095U : code);
}

/**************************************************************************************************
 *                                        MSH 调试
 *
 *     vdda            打印当前标定值
 *     vdda 3600       改成 3600mV（量出来多少就填多少）
 *     vdda 0          恢复默认
 *
 * 量法：让 DAC 输出一个中间值（比如 dacv 1650 → 码值 1849），
 *       万用表量 PA5 得到实测值，代入
 *           VDDA = 实测mV * 4095 / 码值
 *       再用 `vdda <结果>` 写进去。注意量的时候串口别断开，
 *       WCH-Link 的串口在开关瞬间会拉 DTR/RTS，等于给板子复位（DAC 会回 0V）。
 **************************************************************************************************/
static int vdda_cmd(int argc, char **argv)
{
    if(argc > 1)
    {
        vref_set_vdda_mv((uint16_t)atoi(argv[1]));
    }

    rt_kprintf("[VREF] VDDA = %u mV  (code 2048 = %u mV, code 4095 = %u mV)\r\n",
               (unsigned)vref_vdda_mv(),
               (unsigned)vref_code_to_mv(2048),
               (unsigned)vref_code_to_mv(4095));

    return 0;
}
MSH_CMD_EXPORT_ALIAS(vdda_cmd, vdda, get/set VDDA mV: vdda [mV]);

/**************************************************************************************************
 *                                    MSH：PA5 输出固定电压
 *
 *     dacv 1650       让 PA5 输出 1650mV（用标定过的换算）
 *
 * 校验方法：dacv 1650 之后用万用表量 PA5，应该是 1.65V 而不是 1.80V。
 **************************************************************************************************/
static int dacv_cmd(int argc, char **argv)
{
    uint16_t mv;
    uint16_t code;
    int rc;

    if(argc <= 1)
    {
        rt_kprintf("[VREF] usage: dacv <mV>   (0..%u)\r\n", (unsigned)vref_vdda_mv());
        return 0;
    }

    mv   = (uint16_t)atoi(argv[1]);
    code = vref_mv_to_code(mv);
    rc   = dac_write(code);

    rt_kprintf("[VREF] PA5 <- %u mV = code %u (DOR2 %u, VDDA %u mV) rc=%d\r\n",
               (unsigned)mv, (unsigned)code, (unsigned)DAC->DOR2,
               (unsigned)vref_vdda_mv(), rc);
    return 0;
}
MSH_CMD_EXPORT_ALIAS(dacv_cmd, dacv, set PA5 to mV: dacv <mV>);

/**************************************************************************************************
 *                                  MSH：读一次 PA1 / PB1 的电压
 *
 *     adcv            读 PB1（板上 ADC 输入）
 *     adcv pa1        读 PA1（麦克风）
 *
 * 校验方法：PB1 接 GND 应该 0mV，接 3V3 应该 ≈VDDA，和万用表差几个百分点正常。
 **************************************************************************************************/
static int adcv_cmd(int argc, char **argv)
{
    static uint16_t buf[16] __attribute__((aligned(2048)));     /* 不能放栈上：DMA 目标 */
    adc_input_t in = ADC_PB1;
    uint16_t i, mn = 4095, mx = 0;
    uint32_t sum = 0;
    int rc;

    if(argc > 1)
    {
        if(((argv[1][0] == 'p') || (argv[1][0] == 'P')) &&
           ((argv[1][1] == 'a') || (argv[1][1] == 'A')) &&
           (argv[1][2] == '1'))
        {
            in = ADC_PA1;
        }
    }

    rc = wave_capture(in, buf, 16, 0U);
    if(rc != 0)
    {
        rt_kprintf("[VREF] %s read FAIL rc=%d\r\n", (in == ADC_PB1) ? "PB1" : "PA1", rc);
        return 0;
    }

    for(i = 0; i < 16; i++)
    {
        sum += buf[i];
        if(buf[i] < mn) { mn = buf[i]; }
        if(buf[i] > mx) { mx = buf[i]; }
    }

    rt_kprintf("[VREF] %s code=%u -> %u mV  (%u..%u mV, vpp %u mV)\r\n",
               (in == ADC_PB1) ? "PB1" : "PA1",
               (unsigned)(sum / 16U), (unsigned)vref_code_to_mv(sum / 16U),
               (unsigned)vref_code_to_mv(mn), (unsigned)vref_code_to_mv(mx),
               (unsigned)(vref_code_to_mv(mx) - vref_code_to_mv(mn)));

    return 0;
}
MSH_CMD_EXPORT_ALIAS(adcv_cmd, adcv, read PB1 once (adcv pa1 for mic));
