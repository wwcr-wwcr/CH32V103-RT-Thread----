#ifndef BSP_SDIO_H
#define BSP_SDIO_H

#include "bsp_types.h"
typedef enum {
    BSP_SDIO_NO_RESPONSE, BSP_SDIO_SHORT, BSP_SDIO_SHORT_NO_CRC, BSP_SDIO_LONG
} bsp_sdio_response_t;

/* 初始化主控制器并以 1 位总线开始工作。wires=1/4 表示布线宽度，divider 是 CLKDIV 寄存器值。
 * PC8/D0、PC12/CK、PD2/CMD 必需，4 线模式另用 PC9..11；卡端 CMD/DAT 需要外部上拉。
 * SD 时钟=HCLK/(divider+2)，识别阶段应不超过 400 kHz。本工程 HCLK=96 MHz 时可用 238。 */
bsp_status_t bsp_sdio_init(uint8_t wires, uint8_t divider);
/* 设置运行阶段的总线宽度和 CLKDIV。切换前需用卡协议将卡设置为相同宽度。 */
bsp_status_t bsp_sdio_configure(uint8_t width, uint8_t divider);
/* 发送 CMD0..63，arg 为命令参数。response 分别指向 1 或 4 个 uint32_t，无响应时可为空。
 * R3/R4 使用 SHORT_NO_CRC，poll_limit 为非零轮询上限。卡状态位由协议层解析。 */
bsp_status_t bsp_sdio_command(uint8_t cmd, uint32_t arg, bsp_sdio_response_t kind,
                              uint32_t *response, uint32_t poll_limit);
/* 发送 CMD17/CMD24 并传输单个 512 字节块，arg 已按卡类型转换为字节或扇区地址。
 * write 为 0 读、1 写。须完成卡初始化和块长设置；写后由调用方检查卡编程状态。 */
bsp_status_t bsp_sdio_block(uint8_t write, uint32_t arg, uint32_t words[128], uint32_t poll_limit);
/* 关闭主控制器时钟输出、切断控制器电源位并释放总线引脚。 */
bsp_status_t bsp_sdio_de_init(void);
#endif
