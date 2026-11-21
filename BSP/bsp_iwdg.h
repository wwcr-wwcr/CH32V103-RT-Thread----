#ifndef BSP_IWDG_H
#define BSP_IWDG_H
#include "bsp_types.h"
/* 配置独立看门狗。prescaler 使用 IWDG_Prescaler_x，reload 为 0..4095。
 * 假定软件启动模式且该外设由本模块独占。硬件自动启动模式返回 UNSUPPORTED。
 * 配置超时返回 TIMEOUT，若已写入寄存器可用 de_init 重试恢复。 */
bsp_status_t bsp_iwdg_init(uint8_t prescaler, uint16_t reload);
/* 启动倒计时。成功后直到系统复位都须按期喂狗。 */
bsp_status_t bsp_iwdg_start(void);
/* 重装独立看门狗计数器。 */
bsp_status_t bsp_iwdg_feed(void);
/* 启动前可恢复配置；启动后返回 BSP_ERR_UNSUPPORTED，仍可继续喂狗。 */
bsp_status_t bsp_iwdg_de_init(void);
#endif
