#ifndef BSP_RCC_H
#define BSP_RCC_H
#include "bsp_types.h"
/* 读取 SYSCLK/HCLK/PCLK1/PCLK2/ADC 时钟频率，单位 Hz，无需先启用 MCO。 */
bsp_status_t bsp_rcc_read(RCC_ClocksTypeDef *frequencies);
/* 在 PA8 输出已有时钟源。source 为 RCC_MCO_SYSCLK、HSI、HSE 或 PLLCLK_Div2；
 * 来源须已运行。PA8 与板上电机信号共线，使用前核对硬件接线。 */
bsp_status_t bsp_rcc_mco_init(uint8_t source);
/* 停止 MCO 输出并归还 PA8。 */
bsp_status_t bsp_rcc_mco_de_init(void);
#endif
