#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "bsp_types.h"

/* 初始化 8 位 MSB 优先的 SPI 主机。spi 为 SPI1~3，mode 为 CPOL/CPHA 对应的 0~3，
 * max_hz 限制时钟上限，cs 为可空的低有效片选。route=0 时 SCK/MISO/MOSI 为
 * SPI1 PA5/6/7、SPI2 PB13/14/15（板载 NFC 的 CS 为 PB12）、SPI3 PB3/4/5。
 * route=1 时 SPI1 为 PB3/4/5，SPI3 为 PC10/11/12。SPI2 仅支持 route=0。
 * SPI3 和 I²S3 与以太网 MAC 互斥，避免打开以太网后数据引脚自动改变。 */
bsp_status_t bsp_spi_init(SPI_TypeDef *spi, unsigned route, unsigned mode, uint32_t max_hz,
                          const bsp_pin_t *cs);
/* 在同一片选下交换 length 个字节。tx=NULL 时发送 FF，rx=NULL 时丢弃接收。
 * 超时或总线错误将取消此事务并释放片选，接收区可能仅部分有效。 */
bsp_status_t bsp_spi_transfer(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, size_t length);
/* 释放指定 SPI 及已申请的引脚。 */
bsp_status_t bsp_spi_de_init(SPI_TypeDef *spi);

/* 初始化 Philips I²S 16 位立体声主机。SPI2 或 SPI3，receive=0 主发/1 主收。
 * hz 为目标采样率，actual_hz 可空；未输出 MCLK。WS/CK/SD 引脚如下：
 * SPI2 route=0 PB12/13/15；SPI3 route=0 PA15/PB3/PB5，route=1 PA4/PC10/PC12。
 * 与同实例 SPI 互斥，阻塞式单缓冲收发适用于短帧；连续音频需实时供数。 */
bsp_status_t bsp_i2s_init(SPI_TypeDef *spi, unsigned route, unsigned receive,
                          uint32_t hz, uint32_t *actual_hz);
/* 发送/接收交错排列的左、右声道样本。frames 为立体声帧数，数组有 2*frames 个元素。
 * 主发只填 tx，主收只填 rx。轮询期间欠载/溢出返回 IO。 */
bsp_status_t bsp_i2s_transfer(SPI_TypeDef *spi, const uint16_t *tx, uint16_t *rx, size_t frames);
/* 停止对应 I²S 接口并释放引脚和时钟。 */
bsp_status_t bsp_i2s_de_init(SPI_TypeDef *spi);
#endif
