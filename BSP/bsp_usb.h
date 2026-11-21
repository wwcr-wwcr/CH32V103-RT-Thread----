#ifndef BSP_USB_H
#define BSP_USB_H
#include "bsp_types.h"

typedef enum { BSP_USB_FS, BSP_USB_HS } bsp_usb_port_t;
typedef enum { BSP_USB_BULK_IN1, BSP_USB_BULK_OUT2 } bsp_usb_bulk_ep_t;
typedef enum { BSP_USB_LOW_SPEED, BSP_USB_FULL_SPEED, BSP_USB_HIGH_SPEED } bsp_usb_speed_t;
typedef enum { BSP_USB_OUT = 1, BSP_USB_IN = 9, BSP_USB_SETUP = 13 } bsp_usb_token_t;
typedef struct {
    uint8_t address, endpoint, data1;
    bsp_usb_token_t token;
    uint8_t *data;
    uint16_t length;  /* OUT/SETUP 的发送长度，IN 的接收区容量。 */
} bsp_usb_request_t;
typedef struct {
    uint8_t pid;      /* 返回的 USB 协议 PID 低 4 位，0 表示没有有效响应。 */
    uint16_t received;
} bsp_usb_reply_t;
typedef enum { BSP_USB_ACK, BSP_USB_NAK, BSP_USB_STALL } bsp_usb_handshake_t;
typedef struct {
    uint8_t flags;       /* 原始 INT_FG，含复位、事务、挂起标志；HS 另有 SETUP_ACT。 */
    uint8_t transaction; /* 原始 INT_ST，含端点号、令牌类型和 toggle 状态。 */
    uint16_t rx_length;
} bsp_usb_event_t;

/* 初始化 FS(PA11/12) 或 HS(PB6/7) 设备控制器和 EP0，先保持与主机断开。
 * ep0 指向至少 64 字节、4 字节对齐的片内 SRAM，整个使用期内由该控制器独占。
 * 协议层负责描述符、枚举和总线复位后的重新配置。 */
bsp_status_t bsp_usb_init(bsp_usb_port_t port, uint8_t ep0[64]);
/* 连接/断开设备上拉；enabled 取 1/0。应在协议处理机制就绪后连接。 */
bsp_status_t bsp_usb_connect(bsp_usb_port_t port, uint8_t enabled);
/* 设置设备地址 0..127；SET_ADDRESS 需在状态阶段完成后设置。 */
bsp_status_t bsp_usb_set_address(bsp_usb_port_t port, uint8_t address);
/* 读取待处理标志、当前事务及接收长度。返回后 INT_FG 保留，协议层可访问 EP0 缓冲。 */
bsp_status_t bsp_usb_event(bsp_usb_port_t port, bsp_usb_event_t *event);
/* 处理完成后清除指定 INT_FG 位。此函数同步更新 Bulk 收发状态；
 * 总线复位时丢弃待传数据、撤销 Bulk 端点，恢复 EP0 地址及握手。 */
bsp_status_t bsp_usb_ack_event(bsp_usb_port_t port, uint8_t flags);
/* 配置 EP0 下次发送或接收：in=1 发送，data1=0/1 选择 DATA0/DATA1，
 * length 为发送长度（0..64，接收时传 0）；缓冲内容由协议层准备。 */
bsp_status_t bsp_usb_ep0(bsp_usb_port_t port, uint8_t in, uint8_t data1,
                         bsp_usb_handshake_t handshake, uint8_t length);
/* 配置 0x81 IN 或 0x02 OUT Bulk 端点，建议在 SET_CONFIGURATION 中调用。
 * dma/capacity 是独占的 4 字节对齐片内 SRAM 区域，至少全速 64/高速 512 字节。
 * 按当前连接速度配置单包长度，设备描述符中的 wMaxPacketSize 需一致。 */
bsp_status_t bsp_usb_bulk_init(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep, uint8_t *dma, size_t capacity);
/* 提交一个 IN 数据包（允许零长包）。返回后可复用 data，再次提交前应处理传输事件。 */
bsp_status_t bsp_usb_bulk_write(bsp_usb_port_t port, const uint8_t *data, uint16_t length);
/* 复制已确认的 OUT 数据包并重新开放接收。capacity 不足时保留数据并输出实际长度。 */
bsp_status_t bsp_usb_bulk_read(bsp_usb_port_t port, uint8_t *data, size_t capacity, uint16_t *length);
/* 禁用指定端点并释放 DMA 缓冲；若 SIE 正在处理令牌，返回 BUSY，请重试。 */
bsp_status_t bsp_usb_bulk_de_init(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep);
/* 断开总线、停止 DMA，释放引脚及该实例的时钟。 */
bsp_status_t bsp_usb_de_init(bsp_usb_port_t port);

/* 以主机模式启动指定控制器，dma 是 4 字节对齐、持续独占的片内 SRAM；
 * FS 需要 64 字节，HS 需要 512 字节。板上 PB6/PB7 接 USB Hub，
 * 外接线路及 VBUS 供电须支持主机角色；同一控制器的设备和主机模式互斥。 */
bsp_status_t bsp_usb_host_init(bsp_usb_port_t port, uint8_t *dma, size_t capacity);
/* 在线程中对已连接的根端口发复位、启用 SOF 并输出协商速度。插拔去抖由上层完成。 */
bsp_status_t bsp_usb_host_reset(bsp_usb_port_t port, bsp_usb_speed_t *speed);
/* 对当前根端口发一次 SETUP/OUT/IN 事务，不自动重发。poll_limit 是非零轮询上限。
 * reply 给出握手 PID 和接收长度。NAK 返回 BUSY、STALL/错包返回 IO、无响应返回 TIMEOUT。
 * 高速 OUT 的 NYET 表示本包已被接收；后续 PING、Hub 分裂事务及枚举由协议层处理。 */
bsp_status_t bsp_usb_host_transfer(bsp_usb_port_t port, const bsp_usb_request_t *request,
                                   bsp_usb_reply_t *reply, uint32_t poll_limit);
/* 停止主机 SIE 和 DMA，释放端口资源；进行中的事务/复位返回 BUSY。 */
bsp_status_t bsp_usb_host_de_init(bsp_usb_port_t port);
#endif
