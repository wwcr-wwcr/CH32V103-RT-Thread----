#include "bsp_board.h"
#include "bsp_resource.h"
#include <rthw.h>

volatile uint8_t bsp_eth_spi3_lock;
static const void *owners[5][16];
static GPIO_TypeDef *const ports[] = {GPIOA, GPIOB, GPIOC, GPIOD, GPIOE};
static const uint32_t clocks[] = {
    RCC_APB2Periph_GPIOA, RCC_APB2Periph_GPIOB, RCC_APB2Periph_GPIOC,
    RCC_APB2Periph_GPIOD, RCC_APB2Periph_GPIOE
};

int bsp_port_index(GPIO_TypeDef *port)
{
    for (int i = 0; i < 5; ++i)
        if (port == ports[i]) return i;
    return -1;
}

int bsp_pin_index(uint16_t bit)
{
    if (!bit || (bit & (bit - 1U))) return -1;
    return __builtin_ctz((unsigned int)bit);
}

static int reserved(bsp_pin_t p)
{
    if (p.port == GPIOA && (p.pin & (GPIO_Pin_13 | GPIO_Pin_14))) return 1;
    if (p.port == GPIOC && (p.pin & (GPIO_Pin_14 | GPIO_Pin_15))) return 1;
#ifdef RT_USING_UART1
    if (p.port == GPIOA && (p.pin & (GPIO_Pin_9 | GPIO_Pin_10))) return 1;
#endif
#if BSP_BOARD_LCD_ACTIVE
    if (p.port == GPIOC && p.pin == GPIO_Pin_4) return 1;
    if (p.port == GPIOD && (p.pin & 0xFF33U)) return 1;
    if (p.port == GPIOE && (p.pin & 0xFF80U)) return 1;
#endif
#if BSP_BOARD_TOUCH_ACTIVE
    if (p.port == GPIOD && (p.pin & 0x00CCU)) return 1;
#endif
    return 0;
}

long bsp_irq_save(void) { return rt_hw_interrupt_disable(); }
void bsp_irq_restore(long level) { rt_hw_interrupt_enable(level); }

bsp_status_t bsp_try_lock(volatile uint8_t *lock)
{
    long level = bsp_irq_save();
    bsp_status_t result = *lock ? BSP_ERR_BUSY : BSP_OK;
    *lock = 1;
    bsp_irq_restore(level);
    return result;
}

void bsp_unlock(volatile uint8_t *lock)
{
    long level = bsp_irq_save();
    *lock = 0;
    bsp_irq_restore(level);
}

bsp_status_t bsp_pins_claim(const void *owner, const bsp_pin_t *pins, size_t count)
{
    uint16_t seen[5] = {0};
    bsp_status_t result = BSP_OK;
    if (!owner || !pins || !count || count > 80) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    for (size_t i = 0; i < count; ++i) {
        int p = bsp_port_index(pins[i].port);
        int b = bsp_pin_index(pins[i].pin);
        if (p < 0 || b < 0 || (seen[p] & pins[i].pin)) { result = BSP_ERR_ARG; break; }
        if (reserved(pins[i]) || owners[p][b]) { result = BSP_ERR_BUSY; break; }
        seen[p] |= pins[i].pin;
    }
    if (result == BSP_OK)
        for (size_t i = 0; i < count; ++i)
            owners[bsp_port_index(pins[i].port)][bsp_pin_index(pins[i].pin)] = owner;
    bsp_irq_restore(level);
    return result;
}

void bsp_pins_release(const void *owner, const bsp_pin_t *pins, size_t count)
{
    if (!owner || !pins) return;
    long level = bsp_irq_save();
    for (size_t i = 0; i < count; ++i) {
        int p = bsp_port_index(pins[i].port);
        int b = bsp_pin_index(pins[i].pin);
        if (p >= 0 && b >= 0 && owners[p][b] == owner) owners[p][b] = NULL;
    }
    bsp_irq_restore(level);
}

void bsp_pin_config(bsp_pin_t pin, GPIOMode_TypeDef mode)
{
    int port = bsp_port_index(pin.port);
    if (port < 0 || bsp_pin_index(pin.pin) < 0) return;
    GPIO_InitTypeDef config = {pin.pin, GPIO_Speed_50MHz, mode};
    long level = bsp_irq_save();
    RCC_APB2PeriphClockCmd(clocks[port], ENABLE);
    GPIO_Init(pin.port, &config);
    bsp_irq_restore(level);
}

void bsp_pin_floating(bsp_pin_t pin) { bsp_pin_config(pin, GPIO_Mode_IN_FLOATING); }
