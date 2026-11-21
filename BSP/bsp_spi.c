#include "bsp_spi.h"
#include "bsp_bus_internal.h"

enum { FREE, MASTER_SPI, I2S_TX, I2S_RX };
static struct spi_state {
    volatile uint8_t lock;
    uint8_t mode, pin_count;
    bsp_pin_t pins[4];
    uint32_t previous, previous_audio_clock;
    uint16_t control, high_speed, audio_control, audio_divider;
} state[3];

/* 三根 SPI 信号线的顺序为 CK/MISO/MOSI；最后一项为 I²S 专用 WS。 */
static const bsp_pin_t wires[][4] = {
    {{GPIOA, GPIO_Pin_5}, {GPIOA, GPIO_Pin_6}, {GPIOA, GPIO_Pin_7}, {0}},
    {{GPIOB, GPIO_Pin_3}, {GPIOB, GPIO_Pin_4}, {GPIOB, GPIO_Pin_5}, {0}},
    {{GPIOB, GPIO_Pin_13}, {GPIOB, GPIO_Pin_14}, {GPIOB, GPIO_Pin_15}, {GPIOB, GPIO_Pin_12}},
    {{GPIOB, GPIO_Pin_3}, {GPIOB, GPIO_Pin_4}, {GPIOB, GPIO_Pin_5}, {GPIOA, GPIO_Pin_15}},
    {{GPIOC, GPIO_Pin_10}, {GPIOC, GPIO_Pin_11}, {GPIOC, GPIO_Pin_12}, {GPIOA, GPIO_Pin_4}}
};

static int index_of(SPI_TypeDef *spi)
{
    if (spi == SPI1) return 0;
    if (spi == SPI2) return 1;
    if (spi == SPI3) return 2;
    return -1;
}

static uint32_t route_mask(int i)
{
    return i == 0 ? AFIO_PCFR1_SPI1_REMAP : i == 2 ? AFIO_PCFR1_SPI3_REMAP : 0;
}

static void clock_cmd(int i, FunctionalState enable)
{
    if (i == 0) RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, enable);
    else RCC_APB1PeriphClockCmd(i == 1 ? RCC_APB1Periph_SPI2 : RCC_APB1Periph_SPI3, enable);
}

/* 调用者已持有实例锁，且检查完参数。所有引脚一起申请，失败时不改硬件。 */
static bsp_status_t prepare(SPI_TypeDef *spi, int i, unsigned route, unsigned mode, const bsp_pin_t *cs)
{
    struct spi_state *s = &state[i];
    if (s->mode != FREE) return BSP_ERR_BUSY;
    const bsp_pin_t *p = wires[(i == 0 ? 0 : i == 1 ? 2 : 3) + route];
    s->pins[0] = p[0];
    s->pins[1] = mode == MASTER_SPI ? p[1] : p[3];
    s->pins[2] = p[2];
    if (cs) s->pins[3] = *cs;
    s->pin_count = cs ? 4 : 3;
    if (i == 2) {
        /* 开 MAC 时 SPI3/I²S3 的数据引脚会自动换线；两者应全程互斥。 */
        if (bsp_try_lock(&bsp_eth_spi3_lock) != BSP_OK) return BSP_ERR_BUSY;
        if (RCC->AHBPCENR & RCC_AHBPeriph_ETH_MAC) {
            bsp_unlock(&bsp_eth_spi3_lock);
            return BSP_ERR_BUSY;
        }
    }
    bsp_status_t rc = bsp_pins_claim(s, s->pins, s->pin_count);
    if (rc != BSP_OK) {
        if (i == 2) bsp_unlock(&bsp_eth_spi3_lock);
        return rc;
    }
    long level = bsp_irq_save();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    clock_cmd(i, ENABLE);
    SPI_I2S_DeInit(spi);
    uint32_t mask = route_mask(i);
    if (mask) s->previous = bus_set_route(&AFIO->PCFR1, mask, route ? mask : 0);
    bsp_irq_restore(level);
    if (cs) {
        /* 首次配置也先打开端口时钟，再预装高电平，避免片选低脉冲。 */
        bsp_pin_floating(*cs);
        GPIO_SetBits(cs->port, cs->pin);
        bsp_pin_config(*cs, GPIO_Mode_Out_PP);
    }
    bsp_pin_config(s->pins[0], GPIO_Mode_AF_PP);
    bsp_pin_config(s->pins[1], mode == MASTER_SPI ? GPIO_Mode_IN_FLOATING : GPIO_Mode_AF_PP);
    bsp_pin_config(s->pins[2], mode == I2S_RX ? GPIO_Mode_IN_FLOATING : GPIO_Mode_AF_PP);
    s->mode = mode;
    return BSP_OK;
}

bsp_status_t bsp_spi_init(SPI_TypeDef *spi, unsigned route, unsigned mode, uint32_t max_hz,
                          const bsp_pin_t *cs)
{
    int i = index_of(spi);
    RCC_ClocksTypeDef clocks;
    if (i < 0 || route > 1 || (i == 1 && route) || mode > 3 || !max_hz) return BSP_ERR_ARG;
    RCC_GetClocksFreq(&clocks);
    uint32_t clock = i == 0 ? clocks.PCLK2_Frequency : clocks.PCLK1_Frequency;
    unsigned code = 0;
    while (code < 8 && clock > ((uint64_t)max_hz << (code + 1))) ++code;
    if (code == 8) return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = prepare(spi, i, route, MASTER_SPI, cs);
    if (rc == BSP_OK) {
        SPI_InitTypeDef cfg;
        SPI_StructInit(&cfg);
        cfg.SPI_Mode = SPI_Mode_Master;
        cfg.SPI_NSS = SPI_NSS_Soft;
        cfg.SPI_CPOL = (mode & 2) ? SPI_CPOL_High : SPI_CPOL_Low;
        cfg.SPI_CPHA = (mode & 1) ? SPI_CPHA_2Edge : SPI_CPHA_1Edge;
        cfg.SPI_BaudRatePrescaler = code << 3;
        SPI_Init(spi, &cfg);
        SPI_NSSInternalSoftwareConfig(spi, SPI_NSSInternalSoft_Set);
        /* WCH 高速读模式适用于二分频且 SCK 至少 36 MHz。 */
        state[i].high_speed = code == 0 && clock >= 72000000U ? SPI_HSCR_HSRXEN : 0;
        spi->HSCR = state[i].high_speed;
        SPI_Cmd(spi, ENABLE);
        state[i].control = spi->CTLR1;
    }
    bsp_unlock(&state[i].lock);
    return rc;
}

static bsp_status_t wait_flag(SPI_TypeDef *spi, uint16_t flag, int set, uint32_t *budget)
{
    for (;;) {
        uint16_t status = spi->STATR;
        if (status & (SPI_STATR_MODF | SPI_STATR_OVR | SPI_STATR_UDR)) return BSP_ERR_IO;
        if (((status & flag) != 0) == set) return BSP_OK;
        if (!*budget) return BSP_ERR_TIMEOUT;
        --*budget;
    }
}

static void reset_instance(SPI_TypeDef *spi, struct spi_state *s)
{
    long level = bsp_irq_save();
    SPI_I2S_DeInit(spi);
    bsp_irq_restore(level);
    if (s->mode == MASTER_SPI) {
        spi->HSCR = s->high_speed;
        spi->CTLR1 = s->control;
    }
    else {
        spi->I2SPR = s->audio_divider;
        spi->I2SCFGR = s->audio_control;
    }
}

bsp_status_t bsp_spi_transfer(SPI_TypeDef *spi, const uint8_t *tx, uint8_t *rx, size_t length)
{
    int i = index_of(spi);
    if (i < 0 || !length || (!tx && !rx)) return BSP_ERR_ARG;
    struct spi_state *s = &state[i];
    if (bsp_try_lock(&s->lock) != BSP_OK) return BSP_ERR_BUSY;
    if (s->mode != MASTER_SPI) { bsp_unlock(&s->lock); return BSP_ERR_STATE; }
    uint32_t budget = BSP_BUS_POLL_LIMIT;
    bsp_status_t rc = BSP_OK;
    if (s->pin_count == 4) GPIO_ResetBits(s->pins[3].port, s->pins[3].pin);
    for (size_t n = 0; n < length; ++n) {
        rc = wait_flag(spi, SPI_STATR_TXE, 1, &budget);
        if (rc != BSP_OK) break;
        spi->DATAR = tx ? tx[n] : 0xffU;
        rc = wait_flag(spi, SPI_STATR_RXNE, 1, &budget);
        if (rc != BSP_OK) break;
        uint8_t byte = (uint8_t)spi->DATAR;
        if (rx) rx[n] = byte;
    }
    if (rc == BSP_OK) rc = wait_flag(spi, SPI_STATR_BSY, 0, &budget);
    if (s->pin_count == 4) GPIO_SetBits(s->pins[3].port, s->pins[3].pin);
    if (rc != BSP_OK) reset_instance(spi, s);
    bsp_unlock(&s->lock);
    return rc;
}

bsp_status_t bsp_i2s_init(SPI_TypeDef *spi, unsigned route, unsigned receive, uint32_t hz,
                          uint32_t *actual_hz)
{
    int i = index_of(spi);
    RCC_ClocksTypeDef clocks;
    if (i < 1 || route > 1 || (i == 1 && route) || receive > 1 || hz < 8000 || hz > 192000)
        return BSP_ERR_ARG;
    RCC_GetClocksFreq(&clocks);
    uint32_t div = (clocks.SYSCLK_Frequency + 16U * hz) / (32U * hz);
    if (div < 4 || div > 511) return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = prepare(spi, i, route, receive ? I2S_RX : I2S_TX, NULL);
    if (rc == BSP_OK) {
        I2S_InitTypeDef cfg;
        uint32_t source_mask = i == 1 ? RCC_I2S2SRC : RCC_I2S3SRC;
        long level = bsp_irq_save();
        state[i].previous_audio_clock = bus_set_route(&RCC->CFGR2, source_mask, 0);
        bsp_irq_restore(level);
        I2S_StructInit(&cfg);
        cfg.I2S_Mode = receive ? I2S_Mode_MasterRx : I2S_Mode_MasterTx;
        cfg.I2S_AudioFreq = hz;
        I2S_Init(spi, &cfg);
        /* 固定 16 位双声道、无 MCLK，按实际 SYSCLK 四舍五入。 */
        spi->I2SPR = (div >> 1) | ((div & 1U) << 8);
        state[i].audio_control = spi->I2SCFGR;
        state[i].audio_divider = spi->I2SPR;
        if (actual_hz) *actual_hz = clocks.SYSCLK_Frequency / (32U * div);
    }
    bsp_unlock(&state[i].lock);
    return rc;
}

bsp_status_t bsp_i2s_transfer(SPI_TypeDef *spi, const uint16_t *tx, uint16_t *rx, size_t frames)
{
    int i = index_of(spi);
    if (i < 1 || !frames || frames > SIZE_MAX / 2 || (!tx == !rx)) return BSP_ERR_ARG;
    struct spi_state *s = &state[i];
    if (bsp_try_lock(&s->lock) != BSP_OK) return BSP_ERR_BUSY;
    if (s->mode != (rx ? I2S_RX : I2S_TX)) { bsp_unlock(&s->lock); return BSP_ERR_STATE; }
    uint32_t budget = BSP_BUS_POLL_LIMIT;
    size_t n = 0;
    bsp_status_t rc = BSP_OK;
    I2S_Cmd(spi, ENABLE);
    while (n < 2 * frames) {
        rc = wait_flag(spi, tx ? SPI_STATR_TXE : SPI_STATR_RXNE, 1, &budget);
        if (rc != BSP_OK) break;
        if (tx) spi->DATAR = tx[n++];
        else {
            rx[n++] = spi->DATAR;
            if (n == 2 * frames - 1) {
                /* WCH 16 位接收时序：倒数第二个字后至少一个 CK 周期再关 I2SE，
                 * 硬件完成已开始的最后一个字。用 CK 回读，无需占用系统定时器。 */
                bsp_pin_t ck = s->pins[0];
                uint16_t phase = ck.port->INDR & ck.pin;
                for (unsigned edge = 0; edge < 3; ++edge) {
                    while ((ck.port->INDR & ck.pin) == phase && budget) --budget;
                    if (!budget) { rc = BSP_ERR_TIMEOUT; break; }
                    phase ^= ck.pin;
                }
                I2S_Cmd(spi, DISABLE);
                if (rc != BSP_OK) break;
            }
        }
    }
    if (tx && rc == BSP_OK) {
        rc = wait_flag(spi, SPI_STATR_TXE, 1, &budget);
        if (rc == BSP_OK) rc = wait_flag(spi, SPI_STATR_BSY, 0, &budget);
    }
    I2S_Cmd(spi, DISABLE);
    reset_instance(spi, s); /* 清除上次末尾流水线，下一缓冲区从左声道开始。 */
    bsp_unlock(&s->lock);
    return rc;
}

static bsp_status_t close_mode(SPI_TypeDef *spi, int audio)
{
    int i = index_of(spi);
    if (i < 0 || (audio && i == 0)) return BSP_ERR_ARG;
    struct spi_state *s = &state[i];
    if (bsp_try_lock(&s->lock) != BSP_OK) return BSP_ERR_BUSY;
    if (s->mode == FREE || ((s->mode != MASTER_SPI) != audio)) {
        bsp_unlock(&s->lock);
        return BSP_ERR_STATE;
    }
    long level = bsp_irq_save();
    SPI_I2S_DeInit(spi);
    clock_cmd(i, DISABLE);
    uint32_t mask = route_mask(i);
    if (mask) bus_set_route(&AFIO->PCFR1, mask, s->previous);
    if (audio) {
        uint32_t source_mask = i == 1 ? RCC_I2S2SRC : RCC_I2S3SRC;
        bus_set_route(&RCC->CFGR2, source_mask, s->previous_audio_clock);
    }
    bsp_irq_restore(level);
    bus_release_pins(s, s->pins, s->pin_count);
    s->mode = FREE;
    if (i == 2) bsp_unlock(&bsp_eth_spi3_lock);
    bsp_unlock(&s->lock);
    return BSP_OK;
}

bsp_status_t bsp_spi_de_init(SPI_TypeDef *spi) { return close_mode(spi, 0); }
bsp_status_t bsp_i2s_de_init(SPI_TypeDef *spi) { return close_mode(spi, 1); }
