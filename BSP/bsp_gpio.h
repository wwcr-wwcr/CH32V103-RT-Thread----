#ifndef BSP_GPIO_H
#define BSP_GPIO_H
#include "bsp_board.h"

/* 申请单个 GPIO 并设置模式。初始电平为 0/1，在切换输出模式前写入。
 * mode 可用 GPIO_Mode_Out_PP、Out_OD、IPU、IPD、IN_FLOATING、AIN。 */
bsp_status_t bsp_gpio_init(bsp_pin_t pin, GPIOMode_TypeDef mode, uint8_t initial);
/* 解除该 GPIO 的配置，恢复浮空输入并交还引脚。 */
bsp_status_t bsp_gpio_de_init(bsp_pin_t pin);
/* 写入 0/1。仅适用于已经初始化为推挽或开漏输出的引脚。 */
bsp_status_t bsp_gpio_write(bsp_pin_t pin, uint8_t value);
/* 将输入寄存器的物理电平写入 value。模拟输入应使用 ADC 采样。 */
bsp_status_t bsp_gpio_read(bsp_pin_t pin, uint8_t *value);
#endif
