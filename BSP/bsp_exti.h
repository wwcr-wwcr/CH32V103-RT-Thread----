#ifndef BSP_EXTI_H
#define BSP_EXTI_H
#include "bsp_board.h"

typedef enum { BSP_EDGE_RISING, BSP_EDGE_FALLING, BSP_EDGE_BOTH } bsp_edge_t;
/* 申请 pin 和同编号 EXTI 线并启动中断。callback(arg) 在中断上下文执行。
 * 上升沿默认下拉，下降沿默认上拉，双边沿浮空。请在 RT-Thread 板级初始化后调用。 */
bsp_status_t bsp_exti_init(bsp_pin_t pin, bsp_edge_t edge, void (*callback)(void *), void *arg);
/* 停止该引脚的中断，解除回调并恢复浮空输入。共享向量内其他线路继续运行。 */
bsp_status_t bsp_exti_de_init(bsp_pin_t pin);
#endif
