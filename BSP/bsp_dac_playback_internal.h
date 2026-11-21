#ifndef BSP_DAC_PLAYBACK_INTERNAL_H
#define BSP_DAC_PLAYBACK_INTERNAL_H
#include "bsp_types.h"
/* Private ownership of DAC data register, TIM7 and DMA2_CH4. */
bsp_status_t bsp_dac_hold_output(uint8_t channel);
void bsp_dac_release_output(uint8_t channel, uint16_t idle);
bsp_status_t bsp_dac_hold_timer(uint32_t rate, float *actual);
void bsp_dac_release_timer(void);
bsp_status_t bsp_dac_hold_dma(void);
void bsp_dac_release_dma(void);
#endif
