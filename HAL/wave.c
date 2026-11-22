/******************************************************************************
 * 文件名   : wave.c
 * 说明     : ADC 采样实现 —— 只是 adc_capture 的一层薄封装，不做分析
 ******************************************************************************/

#include "wave.h"

int wave_capture(adc_input_t input, uint16_t *buf, uint16_t count, uint32_t rate_hz)
{
    if((buf == 0) || (count == 0))
    {
        return -1;
    }

    return adc_capture(input, buf, count, rate_hz);
}

/* ============================================================================
 * 使用示例：整段复制到你的 task 里
 * ========================================================================== */
#if 0
#include "wave.h"
#include "fft.h"

/* 缓冲在 task 里建：半字对齐、不跨 64 KiB */
static uint16_t s_buf[256] __attribute__((aligned(2048)));

static void tick(void)
{
    /* 只采样，什么分析都不做 */
    if(wave_capture(ADC_PB1, s_buf, 256, 5000) != 0)
    {
        return;                     /* 采失败：保持上一帧 */
    }

    /* 要频谱就交给 HAL/fft，要频率/幅值/偏置就交给 APP/wave_ana */
}
#endif
