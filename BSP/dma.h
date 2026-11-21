#ifndef DMA_H
#define DMA_H
#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif
/* init 开时钟并复位指定通道。调用方用 DMA_Init 配置传输后再 start。 */
void dma_init(DMA_Channel_TypeDef *channel);
void dma_start(DMA_Channel_TypeDef *channel);
void dma_stop(DMA_Channel_TypeDef *channel);

#ifdef __cplusplus
}
#endif
#endif
