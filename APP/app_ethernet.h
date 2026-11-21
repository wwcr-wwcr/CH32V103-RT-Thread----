#ifndef APP_ETHERNET_H
#define APP_ETHERNET_H

#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 打开板载 RJ45 的 10BASE-T 控制器，mac 为本设备的六字节单播 MAC。 */
app_status_t app_ethernet_init(const uint8_t mac[6]);
/* 停止收发并释放板载 PHY 资源。 */
app_status_t app_ethernet_de_init(void);
/* 检查网线链路，connected 返回 0 或 1。 */
app_status_t app_ethernet_link(uint8_t *connected);
/* 提交含以太网头、不含 FCS 的一帧（14..1518 字节）。链路未就绪或发送队列满返回 BUSY。 */
app_status_t app_ethernet_send(const uint8_t *frame, uint16_t length);
/* 取一帧，空队列为 BUSY；缓冲不足返回 ARG 和所需长度，可重试。 */
app_status_t app_ethernet_receive(uint8_t *frame, uint16_t capacity, uint16_t *length);

#ifdef __cplusplus
}
#endif
#endif
