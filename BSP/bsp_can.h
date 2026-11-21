#ifndef BSP_CAN_H
#define BSP_CAN_H

#include "bsp_types.h"

/* 初始化经典 CAN。bus 为 CAN1/2，bitrate 为 bit/s（最高 1 M），mode 用
 * CAN_Mode_Normal 或 Silent/LoopBack/Silent_LoopBack。RX/TX 路由如下：
 * CAN1 route=0 PA11/12，2 PB8/9，3 PD0/1；CAN2 route=0 PB12/13，1 PB5/6。
 * 正常模式需接 CAN 收发器。接收默认全通过，FIFO0。芯片封装标识 4~7 的
 * 官方启动校准涉及两个控制器的滤波器，因此该批次同时只开放一个实例。 */
bsp_status_t bsp_can_init(CAN_TypeDef *bus, unsigned route, uint32_t bitrate, unsigned mode);
/* 发送一个标准/扩展、数据/远程帧；frame 中 IDE/RTR 取库常量，DLC 为 0~8。
 * 有界等待发送完成，超时请求取消；外部线路可能已开始发送。 */
bsp_status_t bsp_can_send(CAN_TypeDef *bus, const CanTxMsg *frame);
/* 从 FIFO0 接收一帧。无到达帧时返回 BUSY，FIFO 溢出返回 IO，可再次调用取帧。 */
bsp_status_t bsp_can_receive(CAN_TypeDef *bus, CanRxMsg *frame);
/* 释放实例、引脚。使用 CAN2 期间保留 CAN1 的共享滤波器时钟。 */
bsp_status_t bsp_can_de_init(CAN_TypeDef *bus);
#endif
