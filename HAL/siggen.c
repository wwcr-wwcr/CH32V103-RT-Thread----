/******************************************************************************
 * 文件名   : siggen.c
 * 说明     : DAC 输出引擎实现 —— 只转发给 BSP/dac，不做任何波形计算
 ******************************************************************************/

#include "siggen.h"
#include "dac.h"
#include <rtthread.h>

uint32_t siggen_rate(uint32_t freq_hz, uint16_t count)
{
    uint64_t rate;

    if((freq_hz == 0) || (count < SIGGEN_POINTS_MIN))
    {
        return 0;
    }

    rate = (uint64_t)freq_hz * (uint64_t)count;

    if(rate > SIGGEN_RATE_MAX)
    {
        return 0;
    }

    return (uint32_t)rate;
}

int siggen_play(const uint16_t *table, uint16_t count, uint32_t sample_rate_hz)
{
    if((table == 0) || (count < SIGGEN_POINTS_MIN))
    {
        return -RT_EINVAL;
    }

    if((sample_rate_hz < 1U) || (sample_rate_hz > SIGGEN_RATE_MAX))
    {
        return -RT_EINVAL;
    }

    return dac_play(table, count, sample_rate_hz);
}

int siggen_stop(void)
{
    return dac_stop();
}

int siggen_level(uint16_t code)
{
    if(code > 4095U)
    {
        return -RT_EINVAL;
    }

    return dac_write(code);
}

/* ============================================================================
 * 使用示例：整段复制到 task 里（本文件只管输出，波形请用 APP/wave_gen）
 * ========================================================================== */
#if 0
#include "siggen.h"

static uint16_t s_table[64];        /* 表由你自己建，填好后交给这里 */

static void out_demo(void)
{
    /* 假设表已经填好一个周期（64 点） */
    uint32_t rate = siggen_rate(1000, 64);      /* 1 kHz → 64000 点/秒 */

    if(rate != 0)
    {
        siggen_play(s_table, 64, rate);
    }

    siggen_stop();                              /* 收尾必须停 */
}
#endif
