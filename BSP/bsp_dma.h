#ifndef BSP_DMA_H
#define BSP_DMA_H

#include "bsp_types.h"

typedef enum {
    BSP_DMA_MEM_TO_MEM, BSP_DMA_MEM_TO_PERIPH, BSP_DMA_PERIPH_TO_MEM
} bsp_dma_direction_t;

/* 申请并复位单个 DMA 通道。channel 取 DMA1_Channel1..7 或 DMA2_Channel1..11。
 * 屏幕使用的 DMA2_Channel2 返回 BSP_ERR_BUSY。 */
bsp_status_t bsp_dma_init(DMA_Channel_TypeDef *channel);
/* 执行一次阻塞传输，src/dst 分别是源/目的地址，count 为元素数，width 为 1/2/4 字节。
 * 内存地址递增、外设地址固定；外设模式要求调用方开启对应 DMA 请求。
 * poll_limit 为最大轮询次数，非零；内存区域须互不重叠。超时/错误后已停止 DMA。 */
bsp_status_t bsp_dma_transfer(DMA_Channel_TypeDef *channel, bsp_dma_direction_t dir,
                              const volatile void *src, volatile void *dst,
                              uint16_t count, uint8_t width, uint32_t poll_limit);
/* 停用并释放该通道；不影响同控制器其他通道。 */
bsp_status_t bsp_dma_de_init(DMA_Channel_TypeDef *channel);

#endif
