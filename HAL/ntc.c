/* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * SPDX-License-Identifier: Apache-2.0 */
#include "ntc.h"
#include "adc.h"
#include "gpio.h"
#include <math.h>

void ntc_init(void)
{
    gpio_config(GPIOB, GPIO_Pin_0, GPIO_Mode_AIN);
}

int32_t ntc_get_temperature(void)
{
    /* 16 个采样值共占 32 字节，按 32 字节对齐可避免 DMA 缓冲区跨越 64 KiB 边界。 */
    uint16_t samples[16] __attribute__((aligned(32)));
    if (adc_capture(ADC_PB0, samples, 16, 0) != 0) return INT32_MIN;

    unsigned sum = 0;
    for (unsigned i = 0; i < 16; ++i) sum += samples[i];
    float code = sum / 16.0f;
    if (code <= 0.0f || code >= 4095.0f) return INT32_MIN;

    /* 上拉电阻为 10 kΩ，NTC 在 25 ℃ 时为 10 kΩ。由分压求得阻值比，再用 B 值 3380 K 换算温度。 */
    float ratio = code / (4096.0f - code);
    float celsius = 1.0f / (1.0f / 298.15f + logf(ratio) / 3380.0f) - 273.15f;
    return (int32_t)(celsius * 10.0f + (celsius < 0.0f ? -0.5f : 0.5f));
}

int32_t ntc_read_celsius(void)
{
    int32_t tenths = ntc_get_temperature();
    return tenths == INT32_MIN ? INT32_MIN : tenths / 10;
}

float ntc_read_celsius_f(void)
{
    int32_t tenths = ntc_get_temperature();
    return tenths == INT32_MIN ? NAN : tenths / 10.0f;
}
