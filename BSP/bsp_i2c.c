#include "bsp_i2c.h"
#include "bsp_bus_internal.h"

static const bsp_pin_t routes[][2] = {
    {{GPIOB, GPIO_Pin_6}, {GPIOB, GPIO_Pin_7}},
    {{GPIOB, GPIO_Pin_8}, {GPIOB, GPIO_Pin_9}},
    {{GPIOB, GPIO_Pin_10}, {GPIOB, GPIO_Pin_11}}
};
static struct {
    volatile uint8_t lock;
    uint8_t active, pins;
    uint32_t hz, previous;
} state[2];

#define I2C_ERRORS (I2C_STAR1_BERR | I2C_STAR1_ARLO | I2C_STAR1_AF | I2C_STAR1_OVR | I2C_STAR1_TIMEOUT)

static int index_of(I2C_TypeDef *bus)
{
    if (bus == I2C1) return 0;
    if (bus == I2C2) return 1;
    return -1;
}

static void configure(I2C_TypeDef *bus, uint32_t hz)
{
    I2C_InitTypeDef config;
    long level = bsp_irq_save();
    I2C_DeInit(bus);
    bsp_irq_restore(level);
    I2C_StructInit(&config);
    config.I2C_ClockSpeed = hz;
    config.I2C_Ack = I2C_Ack_Enable;
    I2C_Init(bus, &config);
}

bsp_status_t bsp_i2c_init(I2C_TypeDef *bus, uint32_t hz, unsigned route)
{
    RCC_ClocksTypeDef clocks;
    int i = index_of(bus);
    if (i < 0 || route > 1 || (i == 1 && route) || !hz || hz > 400000U) return BSP_ERR_ARG;
    RCC_GetClocksFreq(&clocks);
    /* CTLR2.FREQ 的有效范围为 4~60 MHz，不能只把超范围的 PCLK1 截短。 */
    if (clocks.PCLK1_Frequency < 4000000U || clocks.PCLK1_Frequency > 60000000U)
        return BSP_ERR_UNSUPPORTED;
    if (hz <= 100000U && clocks.PCLK1_Frequency / (2U * hz) > 4095U) return BSP_ERR_ARG;
    unsigned pins = i == 0 ? route : 2;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = BSP_ERR_BUSY;
    if (!state[i].active) rc = bsp_pins_claim(&state[i], routes[pins], 2);
    if (rc == BSP_OK) {
        long level = bsp_irq_save();
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
        RCC_APB1PeriphClockCmd(i ? RCC_APB1Periph_I2C2 : RCC_APB1Periph_I2C1, ENABLE);
        if (i == 0)
            state[i].previous = bus_set_route(&AFIO->PCFR1, AFIO_PCFR1_I2C1_REMAP,
                                               route ? AFIO_PCFR1_I2C1_REMAP : 0);
        bsp_irq_restore(level);
        bsp_pin_config(routes[pins][0], GPIO_Mode_AF_OD);
        bsp_pin_config(routes[pins][1], GPIO_Mode_AF_OD);
        configure(bus, hz);
        state[i].pins = pins;
        state[i].hz = hz;
        state[i].active = 1;
    }
    bsp_unlock(&state[i].lock);
    return rc;
}

/* 不提前读 STAR2，由收发路径在需要释放 SCL 时清 ADDR。 */
static bsp_status_t wait_flag(I2C_TypeDef *bus, uint16_t flag, uint32_t *budget)
{
    for (;;) {
        uint16_t status = bus->STAR1;
        if (status & I2C_ERRORS) return BSP_ERR_IO;
        if (status & flag) return BSP_OK;
        if (!*budget) return BSP_ERR_TIMEOUT;
        --*budget;
    }
}

static bsp_status_t send_address(I2C_TypeDef *bus, uint8_t address, uint32_t *budget)
{
    bus->CTLR1 |= I2C_CTLR1_START;
    bsp_status_t rc = wait_flag(bus, I2C_STAR1_SB, budget);
    if (rc != BSP_OK) return rc;
    bus->DATAR = address;
    return wait_flag(bus, I2C_STAR1_ADDR, budget);
}

static bsp_status_t receive_data(I2C_TypeDef *bus, uint8_t *out, size_t n, uint32_t *budget)
{
    bsp_status_t rc;
    long level = bsp_irq_save();
    /* 单字节在清 ADDR 前撤 ACK；双字节先置 POS，清 ADDR 后立即撤 ACK。
     * 后一种顺序确保第一个字节得到 ACK，第二个字节得到 NACK。 */
    if (n == 1) bus->CTLR1 &= ~I2C_CTLR1_ACK;
    if (n == 2) bus->CTLR1 |= I2C_CTLR1_POS;
    (void)bus->STAR1;
    (void)I2C_ReadRegister(bus, I2C_Register_STAR2);
    if (n == 2) bus->CTLR1 &= ~I2C_CTLR1_ACK;
    if (n == 1) bus->CTLR1 |= I2C_CTLR1_STOP;
    bsp_irq_restore(level);
    if (n == 1) {
        rc = wait_flag(bus, I2C_STAR1_RXNE, budget);
        if (rc == BSP_OK) *out = (uint8_t)bus->DATAR;
        return rc;
    }
    while (n > 3) {
        rc = wait_flag(bus, I2C_STAR1_RXNE, budget);
        if (rc != BSP_OK) return rc;
        *out++ = (uint8_t)bus->DATAR;
        --n;
    }
    if (n == 3) {
        rc = wait_flag(bus, I2C_STAR1_BTF, budget);
        if (rc != BSP_OK) return rc;
        bus->CTLR1 &= ~I2C_CTLR1_ACK;
        *out++ = (uint8_t)bus->DATAR;
    }
    rc = wait_flag(bus, I2C_STAR1_BTF, budget);
    if (rc != BSP_OK) return rc;
    level = bsp_irq_save();
    bus->CTLR1 |= I2C_CTLR1_STOP;
    *out++ = (uint8_t)bus->DATAR;
    *out = (uint8_t)bus->DATAR;
    bsp_irq_restore(level);
    return BSP_OK;
}

bsp_status_t bsp_i2c_transfer(I2C_TypeDef *bus, uint8_t address7,
                              const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len)
{
    int i = index_of(bus);
    uint32_t budget = BSP_BUS_POLL_LIMIT;
    bsp_status_t rc = BSP_OK;
    if (i < 0 || address7 > 0x7f || (!tx_len && !rx_len) ||
        (tx_len && !tx) || (rx_len && !rx)) return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    if (!state[i].active) { rc = BSP_ERR_STATE; goto done; }
    while (bus->STAR2 & I2C_STAR2_BUSY) {
        if (!budget--) { rc = BSP_ERR_TIMEOUT; goto done; }
    }
    if (tx_len) {
        rc = send_address(bus, address7 << 1, &budget);
        if (rc != BSP_OK) goto recover;
        (void)bus->STAR2;
        for (size_t n = 0; n < tx_len; ++n) {
            rc = wait_flag(bus, I2C_STAR1_TXE, &budget);
            if (rc != BSP_OK) goto recover;
            bus->DATAR = tx[n];
        }
        rc = wait_flag(bus, I2C_STAR1_BTF, &budget);
        if (rc != BSP_OK) goto recover;
        if (!rx_len) bus->CTLR1 |= I2C_CTLR1_STOP;
    }
    if (rx_len) {
        rc = send_address(bus, (address7 << 1) | 1U, &budget);
        if (rc != BSP_OK) goto recover;
        rc = receive_data(bus, rx, rx_len, &budget);
        if (rc != BSP_OK) goto recover;
    }
    while (bus->CTLR1 & I2C_CTLR1_STOP) {
        if (!budget--) { rc = BSP_ERR_TIMEOUT; goto recover; }
    }
    bus->CTLR1 = (bus->CTLR1 & ~I2C_CTLR1_POS) | I2C_CTLR1_ACK;
    goto done;

recover:
    /* 丢失仲裁后不可替当前主机发 STOP。异常复位仅作用于本控制器。 */
    if (!(bus->STAR1 & I2C_STAR1_ARLO) && (bus->STAR2 & I2C_STAR2_MSL)) {
        bus->STAR1 = (uint16_t)~I2C_ERRORS;
        bus->CTLR1 |= I2C_CTLR1_STOP;
        budget = BSP_BUS_POLL_LIMIT;
        while ((bus->CTLR1 & I2C_CTLR1_STOP) && budget) --budget;
    }
    configure(bus, state[i].hz);
done:
    bsp_unlock(&state[i].lock);
    return rc;
}

bsp_status_t bsp_i2c_de_init(I2C_TypeDef *bus)
{
    int i = index_of(bus);
    if (i < 0) return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    if (!state[i].active) { bsp_unlock(&state[i].lock); return BSP_ERR_STATE; }
    long level = bsp_irq_save();
    I2C_DeInit(bus);
    RCC_APB1PeriphClockCmd(i ? RCC_APB1Periph_I2C2 : RCC_APB1Periph_I2C1, DISABLE);
    if (i == 0) bus_set_route(&AFIO->PCFR1, AFIO_PCFR1_I2C1_REMAP, state[i].previous);
    bsp_irq_restore(level);
    bus_release_pins(&state[i], routes[state[i].pins], 2);
    state[i].active = 0;
    bsp_unlock(&state[i].lock);
    return BSP_OK;
}
