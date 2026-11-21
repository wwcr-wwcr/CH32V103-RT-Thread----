#ifndef BSP_PWR_H
#define BSP_PWR_H
#include "bsp_types.h"
/* 配置并启用电源电压监测器，level 使用 PWR_PVDLevel_MODE0..7。 */
bsp_status_t bsp_pwr_init(uint32_t level);
/* 读取监测结果，below 为 1 表示供电低于选定门限。 */
bsp_status_t bsp_pwr_read(uint8_t *below);
/* 关闭本次启用的 PVD，并归还共享的 PWR 接口时钟。 */
bsp_status_t bsp_pwr_de_init(void);
#endif
