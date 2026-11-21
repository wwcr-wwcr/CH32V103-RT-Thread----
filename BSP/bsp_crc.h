#ifndef BSP_CRC_H
#define BSP_CRC_H
#include "bsp_types.h"
/* 独占硬件 CRC 单元，启用 AHB 时钟。 */
bsp_status_t bsp_crc_init(void);
/* 每次从 0xFFFFFFFF 开始按 32 位字输入，多项式 0x04C11DB7，无反转或末异或。
 * count 是字数，crc 保存计算结果。空输入返回初值。 */
bsp_status_t bsp_crc_words(const uint32_t *words, size_t count, uint32_t *crc);
/* 复位累加器并关闭所申请的 CRC 时钟。 */
bsp_status_t bsp_crc_de_init(void);
#endif
