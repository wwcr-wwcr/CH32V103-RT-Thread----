#ifndef BOARD_TIM_H
#define BOARD_TIM_H
#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif
/* 配置 TIM2/3/6/7/10 更新频率，停在起点。成功返回 0。 */
int tim_prepare(TIM_TypeDef *tim, uint32_t rate);

#ifdef __cplusplus
}
#endif
#endif
