#ifndef BSP_ETH_H
#define BSP_ETH_H
#include "bsp_types.h"

/* 初始化 ESAD2025 板载 10BASE-T PHY 与 MAC。mac 是 6 字节单播地址。
 * 需要 8 MHz HSE、PLL3 的 60 MHz 输出；不兼容的现有 PLL 配置返回 BUSY。
 * 初始化成功表示控制器可用，网线的实际状态由 link 查询。 */
bsp_status_t bsp_eth_init(const uint8_t mac[6]);
/* 立即读取板载 PHY 的物理链路状态，up=0/1；协商完成即同步 MAC 双工模式。 */
bsp_status_t bsp_eth_link(uint8_t *up);
/* 读取或写入板载 PHY (地址 1) 的 Clause 22 寄存器 reg=0..31。 */
bsp_status_t bsp_eth_phy_read(uint8_t reg, uint16_t *value);
bsp_status_t bsp_eth_phy_write(uint8_t reg, uint16_t value);
/* 提交一帧（含以太网头，不含 FCS），length=14..1518；成功表示 DMA 已接收发送请求。
 * 链路未就绪、仍在协商或发送缓冲占用时返回 BUSY。收发期间每 50 ms 自动复查 PHY，
 * 不需要另行调用 link；调用者可在函数返回后复用原始帧缓冲。 */
bsp_status_t bsp_eth_send(const uint8_t *frame, uint16_t length);
/* 尝试读取一帧到 dst，length 返回实际字节数（不含 FCS）。
 * 空队列返回 BUSY；容量不足返回 ARG 并保留帧，可按返回的长度重试。 */
bsp_status_t bsp_eth_receive(uint8_t *dst, uint16_t capacity, uint16_t *length);
/* 停止 MAC/DMA 并复位控制器，释放板载 PHY 与本驱动开启的时钟。 */
bsp_status_t bsp_eth_de_init(void);
#endif
