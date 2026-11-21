#ifndef BSP_DVP_H
#define BSP_DVP_H
#include "bsp_types.h"

/* 初始化 8 位视频采集。row_bytes 为每行采样字节数（4 的倍数），rows 为行数；
 * polarity 为 RB_DVP_V_POLAR/H_POLAR/P_POLAR 的组合。PA9/PA10 与默认控制台复用。
 * 摄像头供电、外部时钟、SCCB 和图像格式由上层配置。 */
bsp_status_t bsp_dvp_init(uint16_t row_bytes, uint16_t rows, uint8_t polarity);
/* 从 DVP FIFO 轮询读取一帧，dst 需要四字节对齐，capacity 以 uint32_t 个数计。
 * poll_limit 为最大轮询次数；高像素时钟可能触发 FIFO 溢出，返回 BSP_ERR_IO。 */
bsp_status_t bsp_dvp_capture(uint32_t *dst, size_t capacity, uint32_t poll_limit);
/* 停止采集并释放 D0..D7、PCLK、HSYNC、VSYNC。 */
bsp_status_t bsp_dvp_de_init(void);
#endif
