#ifndef BSP_ADC_TIMING_INTERNAL_H
#define BSP_ADC_TIMING_INTERNAL_H
#include "bsp_types.h"

/* TIM4 发采样脉冲，TIM8 按更新次数关闭内部计数门；两者均不占 GPIO。 */
bsp_status_t bsp_adc_trigger_claim(uint16_t period_ticks, uint16_t samples);
void bsp_adc_trigger_release(void);
bsp_status_t bsp_adc_frame_dma(ADC_TypeDef *adc, uint16_t *dst, uint16_t count,
                               uint32_t poll_limit, void (*start)(uint32_t), uint32_t frame_ms);
#endif
