#include "bsp_analog_pin.h"

enum { NO_DRIVER, ANALOG_DRIVER, TOUCH_DRIVER };
typedef struct {
    uint8_t inputs;
    uint8_t driver;
} analog_pin_state_t;

/* VCT6 的 ADC、DAC、OPA 模拟复用均位于 PA、PB、PC、PE。 */
static analog_pin_state_t states[4][16];

static analog_pin_state_t *state_for(bsp_pin_t pin)
{
    unsigned port;
    if (pin.port == GPIOA) port = 0;
    else if (pin.port == GPIOB) port = 1;
    else if (pin.port == GPIOC) port = 2;
    else if (pin.port == GPIOE) port = 3;
    else return NULL;
    if (!pin.pin || (pin.pin & (pin.pin - 1U))) return NULL;
    return &states[port][__builtin_ctz((unsigned)pin.pin)];
}

bsp_status_t bsp_analog_pin_claim(bsp_pin_t pin, bsp_analog_role_t role)
{
    analog_pin_state_t *state = state_for(pin);
    bsp_status_t status = BSP_OK;
    long level;
    if (!state || (role != BSP_ANALOG_INPUT && role != BSP_ANALOG_OUTPUT))
        return BSP_ERR_ARG;
    level = bsp_irq_save();
    if (state->driver == TOUCH_DRIVER || (role == BSP_ANALOG_OUTPUT && state->driver))
        status = BSP_ERR_BUSY;
    else {
        if (!state->inputs && !state->driver) {
            status = bsp_pins_claim(state, &pin, 1);
            if (status == BSP_OK) bsp_pin_config(pin, GPIO_Mode_AIN);
        }
        if (status == BSP_OK) {
            if (role == BSP_ANALOG_OUTPUT) state->driver = ANALOG_DRIVER;
            else state->inputs++;
        }
    }
    bsp_irq_restore(level);
    return status;
}

void bsp_analog_pin_release(bsp_pin_t pin, bsp_analog_role_t role)
{
    analog_pin_state_t *state = state_for(pin);
    if (!state) return;
    long level = bsp_irq_save();
    if (role == BSP_ANALOG_OUTPUT && state->driver == ANALOG_DRIVER)
        state->driver = NO_DRIVER;
    else if (role == BSP_ANALOG_INPUT && state->inputs && state->driver != TOUCH_DRIVER)
        state->inputs--;
    else {
        bsp_irq_restore(level);
        return;
    }
    if (!state->inputs && !state->driver) {
        bsp_pin_floating(pin);
        bsp_pins_release(state, &pin, 1);
    }
    bsp_irq_restore(level);
}

bsp_status_t bsp_analog_touch_begin(bsp_pin_t pin)
{
    analog_pin_state_t *state = state_for(pin);
    if (!state) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (state->inputs == 1 && !state->driver) {
        state->driver = TOUCH_DRIVER;
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

void bsp_analog_touch_end(bsp_pin_t pin)
{
    analog_pin_state_t *state = state_for(pin);
    if (!state) return;
    long level = bsp_irq_save();
    if (state->driver == TOUCH_DRIVER) state->driver = NO_DRIVER;
    bsp_irq_restore(level);
}
