#include "bsp_gpio.h"
#include "bsp_resource.h"

enum { UNUSED, INPUT, OUTPUT, ANALOG };
static uint8_t state[5][16];

static uint8_t *pin_state(bsp_pin_t pin)
{
    int port = bsp_port_index(pin.port);
    int bit = bsp_pin_index(pin.pin);
    return port < 0 || bit < 0 ? NULL : &state[port][bit];
}

bsp_status_t bsp_gpio_init(bsp_pin_t pin, GPIOMode_TypeDef mode, uint8_t initial)
{
    uint8_t *s = pin_state(pin);
    int output = mode == GPIO_Mode_Out_PP || mode == GPIO_Mode_Out_OD;
    if (!s || initial > 1 || (!output && mode != GPIO_Mode_IPU && mode != GPIO_Mode_IPD &&
        mode != GPIO_Mode_IN_FLOATING && mode != GPIO_Mode_AIN)) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = bsp_pins_claim(s, &pin, 1);
    if (result == BSP_OK) {
        if (output) {
            bsp_pin_floating(pin); /* 打开端口时钟，再设置输出锁存器。 */
            GPIO_WriteBit(pin.port, pin.pin, (BitAction)initial);
        }
        bsp_pin_config(pin, mode);
        *s = output ? OUTPUT : (mode == GPIO_Mode_AIN ? ANALOG : INPUT);
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_gpio_de_init(bsp_pin_t pin)
{
    uint8_t *s = pin_state(pin);
    if (!s) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    if (*s == UNUSED) { bsp_irq_restore(level); return BSP_ERR_STATE; }
    bsp_pin_floating(pin);
    bsp_pins_release(s, &pin, 1);
    *s = UNUSED;
    bsp_irq_restore(level);
    return BSP_OK;
}

bsp_status_t bsp_gpio_write(bsp_pin_t pin, uint8_t value)
{
    uint8_t *s = pin_state(pin);
    if (!s || value > 1) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = *s == OUTPUT ? BSP_OK : BSP_ERR_STATE;
    if (result == BSP_OK) GPIO_WriteBit(pin.port, pin.pin, (BitAction)value);
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_gpio_read(bsp_pin_t pin, uint8_t *value)
{
    uint8_t *s = pin_state(pin);
    if (!s || !value) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = (*s == INPUT || *s == OUTPUT) ? BSP_OK : BSP_ERR_STATE;
    if (result == BSP_OK) *value = GPIO_ReadInputDataBit(pin.port, pin.pin);
    bsp_irq_restore(level);
    return result;
}
