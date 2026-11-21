#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "bsp_types.h"

/* 初始化硬件主机。bus 为 I2C1/2，hz 推荐 100000 或 400000。
 * I2C1 的 route=1 为板载 PB8(SCL)/PB9(SDA)，route=0 为 PB6/7；
 * I2C2 的 route=0 为 PB10/11，与蓝牙 UART 共用。线路需外部上拉。
 * PCLK1 必须为 4~60 MHz。本板默认 48 MHz，100 kHz 时 CCR=240。 */
bsp_status_t bsp_i2c_init(I2C_TypeDef *bus, uint32_t hz, unsigned route);
/* 执行一次 7 位寻址事务。address7 不含读写位。tx/tx_len 先写，
 * rx/rx_len 后读，两者均非空时使用重复 START。单向传输另一段填 NULL,0。
 * 出错时目标器件可能已收到部分数据，接收数组可能仅部分有效。 */
bsp_status_t bsp_i2c_transfer(I2C_TypeDef *bus, uint8_t address7,
                              const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
/* 释放对应 I2C 实例及 SCL/SDA；进行中的事务返回 BUSY。 */
bsp_status_t bsp_i2c_de_init(I2C_TypeDef *bus);
#endif
