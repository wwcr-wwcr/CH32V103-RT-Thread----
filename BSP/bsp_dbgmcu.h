#ifndef BSP_DBGMCU_H
#define BSP_DBGMCU_H
#include "bsp_types.h"
/* 启用选定外设的调试冻结位，mask 使用 DBGMCU_*_STOP 等宏的组合。 */
bsp_status_t bsp_dbgmcu_init(uint32_t mask);
/* 返回厂商芯片标识，读取无需先配置调试冻结。 */
uint32_t bsp_dbgmcu_chip_id(void);
/* 恢复本模块更改的调试控制位。 */
bsp_status_t bsp_dbgmcu_de_init(void);
#endif
