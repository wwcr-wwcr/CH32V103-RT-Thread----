#ifndef BSP_BKP_H
#define BSP_BKP_H
#include "bsp_types.h"
/* 开启备份寄存器接口，保留备份域内容。 */
bsp_status_t bsp_bkp_init(void);
/* 读取 16 位寄存器，index 为 1..42。 */
bsp_status_t bsp_bkp_read(uint8_t index, uint16_t *value);
/* 写入单个备份寄存器并恢复原先的备份域写保护。 */
bsp_status_t bsp_bkp_write(uint8_t index, uint16_t value);
/* 关闭本次接口，已经写入的值继续保留。 */
bsp_status_t bsp_bkp_de_init(void);
#endif
