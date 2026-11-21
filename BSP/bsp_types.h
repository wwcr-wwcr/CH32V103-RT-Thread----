#ifndef BSP_TYPES_H
#define BSP_TYPES_H

#include <stddef.h>
#include "ch32v30x.h"

/* 所有 BSP 接口共用的返回值。 */
typedef enum {
    BSP_OK = 0, BSP_ERR_ARG = -1, BSP_ERR_BUSY = -2, BSP_ERR_STATE = -3,
    BSP_ERR_TIMEOUT = -4, BSP_ERR_IO = -5, BSP_ERR_UNSUPPORTED = -6
} bsp_status_t;

/* pin 是单个 GPIO_Pin_x 位掩码。 */
typedef struct { GPIO_TypeDef *port; uint16_t pin; } bsp_pin_t;

#endif
