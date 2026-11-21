#ifndef BSP_ANALOG_PIN_H
#define BSP_ANALOG_PIN_H

#include "bsp_resource.h"

/* ADC/DAC/OPA 专用。每个已初始化通道持有一次引用，驱动输出最多一个。 */
typedef enum { BSP_ANALOG_INPUT, BSP_ANALOG_OUTPUT } bsp_analog_role_t;
bsp_status_t bsp_analog_pin_claim(bsp_pin_t pin, bsp_analog_role_t role);
void bsp_analog_pin_release(bsp_pin_t pin, bsp_analog_role_t role);
/* TouchKey 会给电极充电。只有该 ADC 独占输入且没有模拟输出时才能测量。 */
bsp_status_t bsp_analog_touch_begin(bsp_pin_t pin);
void bsp_analog_touch_end(bsp_pin_t pin);
#endif
