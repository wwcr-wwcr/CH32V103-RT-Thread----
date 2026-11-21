#include "bsp_exti.h"
#include "bsp_resource.h"
#include "drv_gpio.h"
#include <drivers/pin.h>

typedef struct {
    GPIO_TypeDef *port;
    void (*callback)(void *);
    void *argument;
} exti_line_t;
static exti_line_t lines[16];

static void dispatch(void *context)
{
    exti_line_t *line = context;
    line->callback(line->argument);
}

bsp_status_t bsp_exti_init(bsp_pin_t pin, bsp_edge_t edge, void (*callback)(void *), void *arg)
{
    int bit = bsp_pin_index(pin.pin);
    int id = ch32_pin_number(pin.port, pin.pin);
    if (bit < 0 || id < 0 || !callback || (unsigned)edge > BSP_EDGE_BOTH) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (!lines[bit].port) {
        result = bsp_pins_claim(&lines[bit], &pin, 1);
        if (result == BSP_OK) {
            rt_err_t err = rt_pin_attach_irq(id, edge, dispatch, &lines[bit]);
            if (err == RT_EOK) {
                err = rt_pin_irq_enable(id, PIN_IRQ_ENABLE);
                if (err != RT_EOK) rt_pin_dettach_irq(id);
            }
            if (err != RT_EOK) {
                bsp_pins_release(&lines[bit], &pin, 1);
                result = err == RT_EBUSY ? BSP_ERR_BUSY : BSP_ERR_IO;
            } else {
                lines[bit].port = pin.port;
                lines[bit].callback = callback;
                lines[bit].argument = arg;
            }
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_exti_de_init(bsp_pin_t pin)
{
    int bit = bsp_pin_index(pin.pin);
    int id = ch32_pin_number(pin.port, pin.pin);
    if (bit < 0 || id < 0) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (lines[bit].port == pin.port) {
        if (rt_pin_irq_enable(id, PIN_IRQ_DISABLE) == RT_EOK && rt_pin_dettach_irq(id) == RT_EOK) {
            bsp_pin_floating(pin);
            bsp_pins_release(&lines[bit], &pin, 1);
            lines[bit] = (exti_line_t){0};
            result = BSP_OK;
        } else result = BSP_ERR_IO;
    }
    bsp_irq_restore(level);
    return result;
}
