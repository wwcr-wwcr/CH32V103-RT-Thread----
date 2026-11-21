#include "bsp_eth.h"
#include "bsp_ahb_internal.h"
#include "ch32v30x_eth.h"
#include <rtthread.h>
#include <string.h>

#ifndef BSP_ETH_POLL_LIMIT
#define BSP_ETH_POLL_LIMIT 1000000u
#endif
#define ETH_GATES (RCC_AHBPeriph_ETH_MAC | RCC_AHBPeriph_ETH_MAC_Tx | RCC_AHBPeriph_ETH_MAC_Rx)
#define PLL3_CONFIG (RCC_PLL3Mul_15 | RCC_PREDIV2_Div2)
#define PLL3_MASK (RCC_PLL3MUL | RCC_PREDIV2)
#define PHY_ADDRESS 1u
#define PHY_STATUS_FULL_10M (1u << 2)
#define PHY_SELECTOR_MASK 0x1Fu
#define PHY_SELECTOR_CSMA_CD 0x01u
#define LINK_POLL_TICKS ((50u * RT_TICK_PER_SECOND + 999u) / 1000u)
static const bsp_pin_t pins[] = {
    {GPIOC, GPIO_Pin_6}, {GPIOC, GPIO_Pin_7}, {GPIOC, GPIO_Pin_8}, {GPIOC, GPIO_Pin_9},
    {GPIOE, GPIO_Pin_6}, {GPIOC, GPIO_Pin_13}
};
#define PIN_COUNT (sizeof(pins) / sizeof(pins[0]))
static uint8_t active, started_pll;
static enum { LINK_UNCHECKED, LINK_DOWN, LINK_NEGOTIATING, LINK_READY } link_state;
static rt_tick_t link_checked_at;
static uint32_t old_pll_config;
static volatile uint8_t lock;
static ETH_DMADESCTypeDef tx_desc, rx_desc;
static uint8_t tx_buffer[ETH_MAX_PACKET_SIZE] __attribute__((aligned(4)));
static uint8_t rx_buffer[ETH_MAX_PACKET_SIZE] __attribute__((aligned(4)));

static bsp_status_t wait_bits(volatile uint32_t *reg, uint32_t mask, uint32_t wanted)
{
    for (uint32_t i = 0; i < BSP_ETH_POLL_LIMIT; ++i)
        if ((*reg & mask) == wanted) return BSP_OK;
    return BSP_ERR_TIMEOUT;
}

static bsp_status_t phy_access(uint8_t write, uint8_t reg, uint16_t *value)
{
    bsp_status_t status = wait_bits(&ETH->MACMIIAR, ETH_MACMIIAR_MB, 0);
    if (status != BSP_OK) return status;
    if (write) ETH->MACMIIDR = *value;
    ETH->MACMIIAR = (ETH->MACMIIAR & ETH_MACMIIAR_CR) | (PHY_ADDRESS << 11) |
                    ((uint32_t)reg << 6) | (write ? ETH_MACMIIAR_MW : 0) | ETH_MACMIIAR_MB;
    status = wait_bits(&ETH->MACMIIAR, ETH_MACMIIAR_MB, 0);
    if (status == BSP_OK && !write) *value = (uint16_t)ETH->MACMIIDR;
    return status;
}

/* 在实例锁内调用。显式查询读取当前链路，普通收发每 50 ms 最多检查一次。 */
static bsp_status_t refresh_link(int force)
{
    rt_tick_t now = rt_tick_get();
    if (!force && link_state != LINK_UNCHECKED && now - link_checked_at < LINK_POLL_TICKS)
        return BSP_OK;

    link_state = LINK_UNCHECKED;
    uint16_t bsr, bcr;
    bsp_status_t rc = phy_access(0, PHY_BSR, &bsr);
    /* 第一次读到的掉线标记可能是锁存值。 */
    if (rc == BSP_OK && !(bsr & PHY_Linked_Status)) rc = phy_access(0, PHY_BSR, &bsr);
    if (rc != BSP_OK) return rc;
    link_checked_at = now;
    if (!(bsr & PHY_Linked_Status)) {
        link_state = LINK_DOWN;
        return BSP_OK;
    }
    rc = phy_access(0, PHY_BCR, &bcr);
    if (rc != BSP_OK) return rc;
    int full = (bcr & PHY_FULLDUPLEX_10M) != 0;
    if (bcr & PHY_AutoNegotiation) {
        if (!(bsr & PHY_AutoNego_Complete)) {
            link_state = LINK_NEGOTIATING;
            return BSP_OK;
        }
        /* EVT 内置 10M PHY：无伙伴能力页时使用半双工；有能力页时
         * 按 PHYSR.Full_10M 和沁恒示例的 selector 兼容处理选择模式。 */
        uint16_t partner, phy_status;
        rc = phy_access(0, PHY_ANLPAR, &partner);
        if (rc != BSP_OK) return rc;
        full = 0;
        if (partner) {
            rc = phy_access(0, PHY_STATUS, &phy_status);
            if (rc != BSP_OK) return rc;
            full = (phy_status & PHY_STATUS_FULL_10M) ||
                   (partner & PHY_SELECTOR_MASK) != PHY_SELECTOR_CSMA_CD;
        }
    }
    uint32_t mac = ETH->MACCR;
    uint32_t updated = full ? mac | ETH_Mode_FullDuplex : mac & ~ETH_Mode_FullDuplex;
    if (mac != updated) ETH->MACCR = updated;
    link_state = LINK_READY;
    return BSP_OK;
}

static void release_pll(void)
{
    if (started_pll) {
        long level = bsp_irq_save();
        uint32_t mco = RCC->CFGR0 & RCC_CFGR0_MCO;
        uint32_t cfg = RCC->CFGR2;
        if (mco == RCC_CFGR0_MCO_PLL3 || mco == RCC_CFGR0_MCO_PLL3D2 ||
            ((RCC->AHBPCENR & RCC_AHBPeriph_RNG) && (cfg & RCC_RNGSRC)) ||
            ((RCC->APB1PCENR & RCC_APB1Periph_SPI2) && (cfg & RCC_I2S2SRC) &&
             (SPI2->I2SCFGR & SPI_I2SCFGR_I2SE)) ||
            ((RCC->APB1PCENR & RCC_APB1Periph_SPI3) && (cfg & RCC_I2S3SRC) &&
             (SPI3->I2SCFGR & SPI_I2SCFGR_I2SE)) ||
            ((cfg & (RCC_ETH1GEN | RCC_ETH1GSRC)) == (RCC_ETH1GEN | RCC_ETH1GSRC_0))) {
            /* 此期间另一个时钟消费者已启用，将运行中的 PLL 留给它。 */
            started_pll = 0;
            bsp_irq_restore(level);
            return;
        }
        RCC_PLL3Cmd(DISABLE);
        bsp_irq_restore(level);
        /* 等待 PLL3 停止后才能恢复其倍频。PLL2 可能仍使用 PREDIV2。 */
        if (wait_bits(&RCC->CTLR, RCC_PLL3RDY, 0) == BSP_OK) {
            level = bsp_irq_save();
            uint32_t mask = RCC->CTLR & RCC_PLL2ON ? RCC_PLL3MUL : PLL3_MASK;
            RCC->CFGR2 = (RCC->CFGR2 & ~mask) | (old_pll_config & mask);
            bsp_irq_restore(level);
        }
        started_pll = 0;
    }
}

static bsp_status_t enable_pll(void)
{
    long level = bsp_irq_save();
    old_pll_config = RCC->CFGR2 & PLL3_MASK;
    if (((RCC->CTLR & RCC_PLL3ON) && old_pll_config != PLL3_CONFIG) ||
        ((RCC->CTLR & RCC_PLL2ON) && (old_pll_config & RCC_PREDIV2) != RCC_PREDIV2_Div2)) {
        bsp_irq_restore(level);
        return BSP_ERR_BUSY;
    }
    if (!(RCC->CTLR & RCC_PLL3ON)) {
        if ((old_pll_config & RCC_PREDIV2) != RCC_PREDIV2_Div2)
            RCC_PREDIV2Config(RCC_PREDIV2_Div2);
        RCC_PLL3Config(RCC_PLL3Mul_15);
        RCC_PLL3Cmd(ENABLE);
        started_pll = 1;
    }
    bsp_irq_restore(level);
    return wait_bits(&RCC->CTLR, RCC_PLL3RDY, RCC_PLL3RDY);
}

static void reset_mac(void)
{
    long level = bsp_irq_save();
    ETH_DeInit();
    bsp_irq_restore(level);
}

static void stop_mac(void)
{
    reset_mac();  /* AHB 复位终止所有 DMA 访问。 */
    long level = bsp_irq_save();
    EXTEN->EXTEN_CTR &= ~EXTEN_ETH_10M_EN;
    RCC_AHBPeriphClockCmd(ETH_GATES, DISABLE);
    bsp_irq_restore(level);
}

static bsp_status_t init_mac(const uint8_t mac[6], uint32_t mdio_divider)
{
    long level = bsp_irq_save();
    RCC_AHBPeriphClockCmd(ETH_GATES, ENABLE);
    EXTEN->EXTEN_CTR |= EXTEN_ETH_10M_EN;
    bsp_irq_restore(level);
    reset_mac();
    ETH_SoftwareReset();
    bsp_status_t status = wait_bits(&ETH->DMABMR, ETH_DMABMR_SR, 0);
    if (status != BSP_OK) return status;
    ETH->MACMIIAR = mdio_divider;
    ETH->MACCR = ETH_Internal_Pull_Up_Res_Enable | ETH_MACCR_RD | ETH_Speed_10M;
    ETH->MACFFR = 0; /* 接收本机单播地址及广播。 */
    ETH->DMAOMR = ETH_DMAOMR_TSF;
    ETH_MACAddressConfig(ETH_MAC_Address0, (uint8_t *)mac);
    uint16_t value = PHY_Reset;
    status = phy_access(1, PHY_BCR, &value);
    if (status != BSP_OK) return status;
    for (uint32_t i = 0; i < 1024; ++i) {
        status = phy_access(0, PHY_BCR, &value);
        if (status != BSP_OK) return status;
        if (!(value & PHY_Reset)) break;
    }
    if (value & PHY_Reset) return BSP_ERR_TIMEOUT;
    value = 2u << 2; /* WCH 内部 PHY，自动 P/N 极性。 */
    status = phy_access(1, PHY_MDIX, &value);
    if (status != BSP_OK) return status;
    status = refresh_link(1);
    if (status != BSP_OK) return status;

    tx_desc.Status = ETH_DMATxDesc_TCH;
    tx_desc.ControlBufferSize = 0;
    tx_desc.Buffer1Addr = (uint32_t)(uintptr_t)tx_buffer;
    tx_desc.Buffer2NextDescAddr = (uint32_t)(uintptr_t)&tx_desc;
    rx_desc.Status = ETH_DMARxDesc_OWN;
    rx_desc.ControlBufferSize = ETH_DMARxDesc_RCH | sizeof(rx_buffer);
    rx_desc.Buffer1Addr = (uint32_t)(uintptr_t)rx_buffer;
    rx_desc.Buffer2NextDescAddr = (uint32_t)(uintptr_t)&rx_desc;
    __asm volatile ("fence iorw, iorw" ::: "memory");
    ETH->DMATDLAR = (uint32_t)(uintptr_t)&tx_desc;
    ETH->DMARDLAR = (uint32_t)(uintptr_t)&rx_desc;
    ETH_Start();
    return BSP_OK;
}

bsp_status_t bsp_eth_init(const uint8_t mac[6])
{
    if (!mac || (mac[0] & 1u) || !(mac[0] | mac[1] | mac[2] | mac[3] | mac[4] | mac[5])) return BSP_ERR_ARG;
    if (HSE_VALUE != 8000000u || !(RCC->CTLR & RCC_HSERDY)) return BSP_ERR_UNSUPPORTED;
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    if (clocks.HCLK_Frequency > 105000000u) return BSP_ERR_UNSUPPORTED;
    uint32_t divider = clocks.HCLK_Frequency <= 35000000u ? ETH_MACMIIAR_CR_Div16 :
                       clocks.HCLK_Frequency <= 60000000u ? ETH_MACMIIAR_CR_Div26 : ETH_MACMIIAR_CR_Div42;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = active || (RCC->AHBPCENR & ETH_GATES) ? BSP_ERR_BUSY :
                          bsp_pins_claim(&active, pins, PIN_COUNT);
    if (status == BSP_OK) {
        status = bsp_try_lock(&bsp_eth_spi3_lock);
        if (status != BSP_OK) {
            bsp_pins_release(&active, pins, PIN_COUNT);
            bsp_unlock(&lock);
            return status;
        }
        status = enable_pll();
        if (status == BSP_OK) {
            for (size_t i = 0; i < 4; ++i) bsp_pin_config(pins[i], GPIO_Mode_IN_FLOATING);
            for (size_t i = 4; i < PIN_COUNT; ++i) bsp_pin_config(pins[i], GPIO_Mode_AF_PP);
            status = init_mac(mac, divider);
            if (status == BSP_OK) active = 1;
            else {
                stop_mac();
                bsp_ahb_release_pins(&active, pins, PIN_COUNT);
            }
        } else {
            bsp_pins_release(&active, pins, PIN_COUNT);
        }
        if (status != BSP_OK) {
            release_pll();
            bsp_unlock(&bsp_eth_spi3_lock);
        }
    }
    bsp_unlock(&lock);
    return status;
}

static bsp_status_t access_active(uint8_t write, uint8_t reg, uint16_t *value)
{
    if (reg > 31 || !value) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = active ? phy_access(write, reg, value) : BSP_ERR_STATE;
    if (active && write) link_state = LINK_UNCHECKED;
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_eth_phy_read(uint8_t reg, uint16_t *value)
{
    return access_active(0, reg, value);
}

bsp_status_t bsp_eth_phy_write(uint8_t reg, uint16_t value)
{
    return access_active(1, reg, &value);
}

bsp_status_t bsp_eth_link(uint8_t *up)
{
    if (!up) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = active ? refresh_link(1) : BSP_ERR_STATE;
    if (status == BSP_OK) *up = link_state != LINK_DOWN;
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_eth_send(const uint8_t *frame, uint16_t length)
{
    if (!frame || length < ETH_HEADER || length > 1518) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = active ? refresh_link(0) : BSP_ERR_STATE;
    if (status == BSP_OK) {
        status = BSP_ERR_BUSY;
        if (link_state == LINK_READY && !(tx_desc.Status & ETH_DMATxDesc_OWN)) {
            memcpy(tx_buffer, frame, length);
            tx_desc.ControlBufferSize = length;
            __asm volatile ("fence iorw, iorw" ::: "memory");
            tx_desc.Status = ETH_DMATxDesc_TCH | ETH_DMATxDesc_FS | ETH_DMATxDesc_LS | ETH_DMATxDesc_OWN;
            __asm volatile ("fence iorw, iorw" ::: "memory");
            ETH->DMASR = ETH_DMASR_TBUS;
            ETH->DMATPDR = 0;
            status = BSP_OK;
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_eth_receive(uint8_t *dst, uint16_t capacity, uint16_t *length)
{
    if (!dst || !length) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (active) {
        status = refresh_link(0);
        uint32_t flags = rx_desc.Status;
        if (status == BSP_OK) status = BSP_ERR_BUSY;
        /* 可在掉线后取回 DMA 已经收到的完整帧。 */
        if (!(flags & ETH_DMARxDesc_OWN)) {
            __asm volatile ("fence iorw, iorw" ::: "memory");
            unsigned size = (flags & ETH_DMARxDesc_FL) >> ETH_DMARXDESC_FRAME_LENGTHSHIFT;
            if ((flags & (ETH_DMARxDesc_ES | ETH_DMARxDesc_FS | ETH_DMARxDesc_LS)) !=
                (ETH_DMARxDesc_FS | ETH_DMARxDesc_LS) || size < ETH_CRC || size > sizeof(rx_buffer))
                status = BSP_ERR_IO;
            else {
                *length = size - ETH_CRC;
                status = capacity < *length ? BSP_ERR_ARG : BSP_OK;
                if (status == BSP_OK) memcpy(dst, rx_buffer, *length);
            }
            if (status != BSP_ERR_ARG) {
                __asm volatile ("fence iorw, iorw" ::: "memory");
                rx_desc.Status = ETH_DMARxDesc_OWN;
                __asm volatile ("fence iorw, iorw" ::: "memory");
                ETH->DMASR = ETH_DMASR_RBUS;
                ETH->DMARPDR = 0;
            }
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_eth_de_init(void)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (active) {
        stop_mac();
        release_pll();
        bsp_ahb_release_pins(&active, pins, PIN_COUNT);
        bsp_unlock(&bsp_eth_spi3_lock);
        active = 0;
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}
