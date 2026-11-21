#ifndef BSP_WWDG_H
#define BSP_WWDG_H
#include "bsp_types.h"
/* 配置窗口看门狗。prescaler 用 WWDG_Prescaler_x，0x40 < window <= reload <= 0x7F。 */
bsp_status_t bsp_wwdg_init(uint32_t prescaler, uint8_t window, uint8_t reload);
/* 启动窗口倒计时。超出允许时间会触发芯片复位。 */
bsp_status_t bsp_wwdg_start(void);
/* 仅在计数值低于 window 且至少为 0x40 时重装，过早返回 BUSY。必须在计数低于 0x40 之前调用。 */
bsp_status_t bsp_wwdg_feed(void);
/* 启动前释放配置；启动后返回 UNSUPPORTED，保留运行态。 */
bsp_status_t bsp_wwdg_de_init(void);
#endif
