#ifndef BOARD_GPIO_H
#define BOARD_GPIO_H
#include "ch32v30x.h"

#ifdef __cplusplus
extern "C" {
#endif
void gpio_config(GPIO_TypeDef *port, uint16_t pins, GPIOMode_TypeDef mode);

#ifdef __cplusplus
}
#endif
#endif
