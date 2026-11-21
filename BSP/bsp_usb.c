#include "bsp_usb.h"
#include "bsp_ahb_internal.h"
#include "ch32v30x_usb.h"
#include <string.h>
#include <rtthread.h>

static const bsp_pin_t pins[][2] = {
    {{GPIOA, GPIO_Pin_11}, {GPIOA, GPIO_Pin_12}},
    {{GPIOB, GPIO_Pin_6}, {GPIOB, GPIO_Pin_7}}
};
static uint8_t *buffers[2];
static uint8_t host_mode[2], host_ready[2];
static volatile uint8_t host_busy[2];
static bsp_usb_speed_t host_speed[2];
static volatile uint8_t lock;
static uint32_t saved_clock_bits;
#define USB_CLOCK_BITS (RCC_USBHSDIV | RCC_USBHSPLLSRC | RCC_USBHSCLK | RCC_USBHSPLL | RCC_USBFSSRC)
#define FS_FLAGS (USBFS_UIF_BUS_RST | USBFS_UIF_TRANSFER | USBFS_UIF_SUSPEND)
#define HS_FLAGS (USBHS_UIF_BUS_RST | USBHS_UIF_TRANSFER | USBHS_UIF_SUSPEND | USBHS_UIF_SETUP_ACT)
typedef struct {
    uint8_t *dma;
    uint16_t packet_size, received;
    uint8_t pending, data1;
} bulk_endpoint_t;
static bulk_endpoint_t bulk[2][2];

static int overlaps(uintptr_t addr, size_t size, const uint8_t *other, size_t other_size)
{
    return other && addr < (uintptr_t)other + other_size && (uintptr_t)other < addr + size;
}

static bsp_status_t check_buffer(uint8_t *ptr, size_t bytes)
{
    uintptr_t addr = (uintptr_t)ptr;
    if ((addr & 3u) || addr < SRAM_BASE || addr > SRAM_BASE + 0x10000u - bytes) return BSP_ERR_ARG;
    for (unsigned p = 0; p < 2; ++p) {
        if (overlaps(addr, bytes, buffers[p], p == BSP_USB_HS && host_mode[p] ? 512 : 64)) return BSP_ERR_BUSY;
        for (unsigned ep = 0; ep < 2; ++ep)
            if (overlaps(addr, bytes, bulk[p][ep].dma, bulk[p][ep].packet_size)) return BSP_ERR_BUSY;
    }
    return BSP_OK;
}

static bsp_status_t lock_active(bsp_usb_port_t port)
{
    if ((unsigned)port > BSP_USB_HS) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    if (buffers[port] && !host_mode[port]) return BSP_OK;
    bsp_unlock(&lock);
    return BSP_ERR_STATE;
}

static void reset_engine(bsp_usb_port_t port)
{
    if (port == BSP_USB_FS) USBFSD->BASE_CTRL = USBFS_UC_RESET_SIE | USBFS_UC_CLR_ALL;
    else USBHSD->CONTROL = USBHS_UC_RESET_SIE | USBHS_UC_CLR_ALL;
    /* EVT 规定复位脉冲至少 10 us；不能占用 RT-Thread 正在使用的 SysTick。 */
    for (uint32_t cycles = SystemCoreClock / 100000u + 1; cycles; --cycles)
        __asm volatile ("nop");
    if (port == BSP_USB_FS) USBFSD->BASE_CTRL = 0;
    else USBHSD->CONTROL = 0;
}

static void init_ep0(bsp_usb_port_t port, uint8_t *ep0)
{
    if (port == BSP_USB_FS) {
        USBFSD->UEP4_1_MOD = 0;
        USBFSD->UEP2_3_MOD = 0;
        USBFSD->UEP5_6_MOD = 0;
        USBFSD->UEP7_MOD = 0;
        USBFSD->UEP0_DMA = (uint32_t)(uintptr_t)ep0;
        USBFSD->UEP0_TX_LEN = 0;
        USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
        USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        USBFSD->DEV_ADDR = 0;
        USBFSD->INT_FG = FS_FLAGS;
        USBFSD->INT_EN = FS_FLAGS;
        USBFSD->UDEV_CTRL = USBFS_UD_PD_DIS | USBFS_UD_PORT_EN;
        USBFSD->BASE_CTRL = USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
    } else {
        USBHSD->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
        USBHSD->CONTROL = USBHS_UC_DMA_EN | USBHS_UC_INT_BUSY | USBHS_UC_SPEED_HIGH;
        USBHSD->ENDP_CONFIG = 0;
        USBHSD->UEP0_MAX_LEN = 64;
        USBHSD->UEP0_DMA = (uint32_t)(uintptr_t)ep0;
        USBHSD->UEP0_TX_LEN = 0;
        USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
        USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
        USBHSD->DEV_AD = 0;
        USBHSD->INT_FG = HS_FLAGS;
        USBHSD->INT_EN = HS_FLAGS;
    }
}

static void init_host_engine(bsp_usb_port_t port, uint8_t *dma)
{
    uint32_t addr = (uint32_t)(uintptr_t)dma;
    if (port == BSP_USB_FS) {
        USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE;
        USBFSH->HOST_CTRL = 0;
        USBFSH->DEV_ADDR = 0;
        USBFSH->HOST_EP_MOD = USBFS_UH_EP_RX_EN | USBFS_UH_EP_TX_EN;
        USBFSH->HOST_RX_DMA = addr;
        USBFSH->HOST_TX_DMA = addr;
        USBFSH->HOST_TX_CTRL = USBFSH->HOST_RX_CTRL = 0;
        USBFSH->BASE_CTRL = USBFS_UC_HOST_MODE | USBFS_UC_INT_BUSY | USBFS_UC_DMA_EN;
        USBFSH->INT_FG = USBFS_UIF_TRANSFER | USBFS_UIF_DETECT;
        USBFSH->INT_EN = USBFS_UIE_TRANSFER | USBFS_UIE_DETECT;
    } else {
        USBHSH->CONTROL = USBHS_UC_HOST_MODE | USBHS_UC_SPEED_HIGH | USBHS_UC_INT_BUSY | USBHS_UC_DMA_EN;
        USBHSH->HOST_EP_CONFIG = USBHS_UH_EP_RX_EN | USBHS_UH_EP_TX_EN;
        USBHSH->HOST_RX_DMA = addr;
        USBHSH->HOST_TX_DMA = addr;
        USBHSH->HOST_RX_MAX_LEN = 512;
        USBHSH->HOST_CTRL = USBHS_UH_PHY_SUSPENDM;
        USBHSH->INT_FG = USBHS_UIF_TRANSFER | USBHS_UIF_DETECT;
        USBHSH->INT_EN = USBHS_UIE_TRANSFER | USBHS_UIE_DETECT;
    }
}

static bsp_status_t init_port(bsp_usb_port_t port, uint8_t *dma, uint8_t host)
{
    if ((unsigned)port > BSP_USB_HS) return BSP_ERR_ARG;
    if (HSE_VALUE != 8000000u || !(RCC->CTLR & RCC_HSERDY)) return BSP_ERR_UNSUPPORTED;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    int first_port = !buffers[1 - port];
    bsp_status_t status = check_buffer(dma, host && port == BSP_USB_HS ? 512 : 64);
    /* 首个实例独占两条 USB 时钟的配置，避免接管其他栈正使用的 PHY。 */
    if (buffers[port] || (first_port && (RCC->AHBPCENR & (RCC_AHBPeriph_USBHS | RCC_AHBPeriph_USBFS))) ||
        (first_port && (RCC->CFGR2 & RCC_USBHSPLL))) status = BSP_ERR_BUSY;
    if (status == BSP_OK) {
        status = bsp_pins_claim(&buffers[port], pins[port], 2);
        if (status == BSP_OK) {
            /* FS 也使用 USBPHY 48 MHz，两个控制器共同持有 USBHS 的 AHB 时钟。 */
            long level = bsp_irq_save();
            if (first_port) {
                saved_clock_bits = RCC->CFGR2 & USB_CLOCK_BITS;
                RCC_USBCLK48MConfig(RCC_USBCLK48MCLKSource_USBPHY);
                RCC_USBHSPLLCLKConfig(RCC_HSBHSPLLCLKSource_HSE);
                RCC_USBHSConfig(RCC_USBPLL_Div2);
                RCC_USBHSPLLCKREFCLKConfig(RCC_USBHSPLLCKREFCLK_4M);
                RCC_USBHSPHYPLLALIVEcmd(ENABLE);
                RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, ENABLE);
            }
            if (port == BSP_USB_FS) RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, ENABLE);
            bsp_irq_restore(level);
            bsp_pin_config(pins[port][0], GPIO_Mode_IN_FLOATING);
            bsp_pin_config(pins[port][1], GPIO_Mode_IN_FLOATING);
            reset_engine(port);
            if (host) init_host_engine(port, dma);
            else {
                init_ep0(port, dma);
                if (port == BSP_USB_HS) {
                    USBHSD->BUF_MODE = 0;
                    USBHSD->ENDP_TYPE = 0;
                }
            }
            buffers[port] = dma;
            host_mode[port] = host;
            host_ready[port] = 0;
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_usb_init(bsp_usb_port_t port, uint8_t ep0[64])
{
    return init_port(port, ep0, 0);
}

bsp_status_t bsp_usb_host_init(bsp_usb_port_t port, uint8_t *dma, size_t capacity)
{
    if (capacity < (port == BSP_USB_HS ? 512u : 64u)) return BSP_ERR_ARG;
    return init_port(port, dma, 1);
}

/* FS 每个物理端点采用单向单缓冲，EP1_TX 和 EP2_RX 分别拥有独立 DMA 基址。 */
static void disable_bulk(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep)
{
    if (port == BSP_USB_FS) {
        if (ep == BSP_USB_BULK_IN1) USBFSD->UEP4_1_MOD &= ~USBFS_UEP1_TX_EN;
        else USBFSD->UEP2_3_MOD &= ~USBFS_UEP2_RX_EN;
    } else {
        USBHSD->ENDP_CONFIG &= ~(ep == BSP_USB_BULK_IN1 ? USBHS_UEP1_T_EN : USBHS_UEP2_R_EN);
    }
    bulk[port][ep] = (bulk_endpoint_t){0};
}

static void bulk_handshake(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep, uint8_t nak)
{
    uint8_t control = nak ? USBFS_UEP_T_RES_NAK : USBFS_UEP_T_RES_ACK;
    if (bulk[port][ep].data1) control |= port == BSP_USB_FS ? USBFS_UEP_T_TOG : USBHS_UEP_T_TOG_DATA1;
    if (port == BSP_USB_FS) {
        if (ep == BSP_USB_BULK_IN1) USBFSD->UEP1_TX_CTRL = control;
        else USBFSD->UEP2_RX_CTRL = control;
    } else {
        if (ep == BSP_USB_BULK_IN1) USBHSD->UEP1_TX_CTRL = control;
        else USBHSD->UEP2_RX_CTRL = control;
    }
}

bsp_status_t bsp_usb_bulk_init(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep, uint8_t *dma, size_t capacity)
{
    if ((unsigned)ep > BSP_USB_BULK_OUT2) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    uint16_t max_packet = port == BSP_USB_HS &&
        (USBHSD->SPEED_TYPE & USBHS_USB_SPEED_TYPE) == USBHS_USB_SPEED_HIGH ? 512 : 64;
    if (bulk[port][ep].dma) status = BSP_ERR_BUSY;
    else if (capacity < max_packet) status = BSP_ERR_ARG;
    else status = check_buffer(dma, max_packet);
    if (status == BSP_OK) {
        bulk[port][ep].dma = dma;
        bulk[port][ep].packet_size = max_packet;
        bulk_handshake(port, ep, ep == BSP_USB_BULK_IN1);
        if (port == BSP_USB_FS) {
            if (ep == BSP_USB_BULK_IN1) {
                USBFSD->UEP1_TX_LEN = 0;
                USBFSD->UEP1_DMA = (uint32_t)(uintptr_t)dma;
                USBFSD->UEP4_1_MOD |= USBFS_UEP1_TX_EN;
            } else {
                USBFSD->UEP2_DMA = (uint32_t)(uintptr_t)dma;
                USBFSD->UEP2_3_MOD |= USBFS_UEP2_RX_EN;
            }
        } else if (ep == BSP_USB_BULK_IN1) {
            USBHSD->UEP1_TX_LEN = 0;
            USBHSD->UEP1_MAX_LEN = max_packet;
            USBHSD->UEP1_TX_DMA = (uint32_t)(uintptr_t)dma;
            USBHSD->ENDP_CONFIG |= USBHS_UEP1_T_EN;
        } else {
            USBHSD->UEP2_MAX_LEN = max_packet;
            USBHSD->UEP2_RX_DMA = (uint32_t)(uintptr_t)dma;
            USBHSD->ENDP_CONFIG |= USBHS_UEP2_R_EN;
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_usb_bulk_write(bsp_usb_port_t port, const uint8_t *data, uint16_t length)
{
    if (!data && length) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    bulk_endpoint_t *tx = &bulk[port][BSP_USB_BULK_IN1];
    if (!tx->dma) status = BSP_ERR_STATE;
    else if (length > tx->packet_size) status = BSP_ERR_ARG;
    else if (tx->pending) status = BSP_ERR_BUSY;
    else {
        if (length) memmove(tx->dma, data, length);
        if (port == BSP_USB_FS) USBFSD->UEP1_TX_LEN = length;
        else USBHSD->UEP1_TX_LEN = length;
        __asm volatile ("fence iorw, iorw" ::: "memory");
        tx->pending = 1;
        bulk_handshake(port, BSP_USB_BULK_IN1, 0);
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_usb_bulk_read(bsp_usb_port_t port, uint8_t *data, size_t capacity, uint16_t *length)
{
    if (!length || (!data && capacity)) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    bulk_endpoint_t *rx = &bulk[port][BSP_USB_BULK_OUT2];
    if (!rx->dma) status = BSP_ERR_STATE;
    else if (!rx->pending) status = BSP_ERR_BUSY;
    else {
        *length = rx->received;
        if (rx->received > rx->packet_size) status = BSP_ERR_IO;
        else if (capacity < rx->received) status = BSP_ERR_ARG;
        else if (rx->received) memmove(data, rx->dma, rx->received);
        if (status != BSP_ERR_ARG) {
            rx->pending = 0;
            bulk_handshake(port, BSP_USB_BULK_OUT2, 0);
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_usb_bulk_de_init(bsp_usb_port_t port, bsp_usb_bulk_ep_t ep)
{
    if ((unsigned)ep > BSP_USB_BULK_OUT2) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    if (!bulk[port][ep].dma) status = BSP_ERR_STATE;
    else {
        bulk_handshake(port, ep, 1);
        uint8_t sie = port == BSP_USB_FS ? USBFSD->MIS_ST : USBHSD->MIS_ST;
        uint8_t flags = port == BSP_USB_FS ? USBFSD->INT_FG : USBHSD->INT_FG;
        uint8_t transaction = port == BSP_USB_FS ? USBFSD->INT_ST : USBHSD->INT_ST;
        if (!(sie & USBFS_UMS_SIE_FREE) ||
            ((flags & USBFS_UIF_TRANSFER) && (transaction & USBFS_UIS_ENDP_MASK) == (unsigned)ep + 1))
            status = BSP_ERR_BUSY;
        else disable_bulk(port, ep);
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_usb_connect(bsp_usb_port_t port, uint8_t enabled)
{
    if (enabled > 1) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    if (port == BSP_USB_FS) {
        if (enabled) USBFSD->BASE_CTRL |= USBFS_UC_DEV_PU_EN;
        else USBFSD->BASE_CTRL &= ~USBFS_UC_DEV_PU_EN;
    } else {
        if (enabled) USBHSD->CONTROL |= USBHS_UC_DEV_PU_EN;
        else USBHSD->CONTROL &= ~USBHS_UC_DEV_PU_EN;
    }
    bsp_unlock(&lock);
    return BSP_OK;
}

bsp_status_t bsp_usb_set_address(bsp_usb_port_t port, uint8_t address)
{
    if (address > 127) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    if (port == BSP_USB_FS) USBFSD->DEV_ADDR = address;
    else USBHSD->DEV_AD = address;
    bsp_unlock(&lock);
    return BSP_OK;
}

bsp_status_t bsp_usb_event(bsp_usb_port_t port, bsp_usb_event_t *event)
{
    if (!event) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    if (port == BSP_USB_FS) {
        event->flags = USBFSD->INT_FG & FS_FLAGS;
        event->transaction = USBFSD->INT_ST;
        event->rx_length = USBFSD->RX_LEN;
    } else {
        event->flags = USBHSD->INT_FG & HS_FLAGS;
        event->transaction = USBHSD->INT_ST;
        event->rx_length = USBHSD->RX_LEN;
    }
    __asm volatile ("fence iorw, iorw" ::: "memory");
    bsp_unlock(&lock);
    return BSP_OK;
}

bsp_status_t bsp_usb_ack_event(bsp_usb_port_t port, uint8_t flags)
{
    if (flags & ~(port == BSP_USB_FS ? FS_FLAGS : HS_FLAGS)) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    flags &= port == BSP_USB_FS ? USBFSD->INT_FG : USBHSD->INT_FG;
    if (flags & USBFS_UIF_BUS_RST) {
        disable_bulk(port, BSP_USB_BULK_IN1);
        disable_bulk(port, BSP_USB_BULK_OUT2);
        if (port == BSP_USB_FS) {
            USBFSD->DEV_ADDR = 0;
            USBFSD->UEP0_TX_CTRL = USBFS_UEP_T_RES_NAK;
            USBFSD->UEP0_RX_CTRL = USBFS_UEP_R_RES_ACK;
        } else {
            USBHSD->DEV_AD = 0;
            USBHSD->UEP0_TX_CTRL = USBHS_UEP_T_RES_NAK;
            USBHSD->UEP0_RX_CTRL = USBHS_UEP_R_RES_ACK;
        }
    } else if (flags & USBFS_UIF_TRANSFER) {
        uint8_t transaction = port == BSP_USB_FS ? USBFSD->INT_ST : USBHSD->INT_ST;
        uint8_t token_ep = transaction & (USBFS_UIS_TOKEN_MASK | USBFS_UIS_ENDP_MASK);
        if (token_ep == (USBFS_UIS_TOKEN_IN | 1) && !(transaction & USBFS_UIS_IS_NAK) &&
            bulk[port][BSP_USB_BULK_IN1].pending) {
            bulk[port][BSP_USB_BULK_IN1].pending = 0;
            bulk[port][BSP_USB_BULK_IN1].data1 ^= 1;
            bulk_handshake(port, BSP_USB_BULK_IN1, 1);
        } else if (token_ep == (USBFS_UIS_TOKEN_OUT | 2) && (transaction & USBFS_UIS_TOG_OK) &&
                   !(transaction & USBFS_UIS_IS_NAK) && bulk[port][BSP_USB_BULK_OUT2].dma) {
            bulk_endpoint_t *rx = &bulk[port][BSP_USB_BULK_OUT2];
            rx->received = port == BSP_USB_FS ? USBFSD->RX_LEN : USBHSD->RX_LEN;
            __asm volatile ("fence iorw, iorw" ::: "memory");
            rx->pending = 1;
            rx->data1 ^= 1;
            bulk_handshake(port, BSP_USB_BULK_OUT2, 1);
        }
    }
    if (port == BSP_USB_FS) USBFSD->INT_FG = flags;
    else USBHSD->INT_FG = flags;
    bsp_unlock(&lock);
    return BSP_OK;
}

bsp_status_t bsp_usb_ep0(bsp_usb_port_t port, uint8_t in, uint8_t data1,
                         bsp_usb_handshake_t handshake, uint8_t length)
{
    if (in > 1 || data1 > 1 || length > 64 || (!in && length) ||
        (unsigned)handshake > BSP_USB_STALL) return BSP_ERR_ARG;
    bsp_status_t status = lock_active(port);
    if (status != BSP_OK) return status;
    uint8_t ctrl = handshake == BSP_USB_ACK ? 0 : handshake == BSP_USB_NAK ? 2 : 3;
    if (data1) ctrl |= port == BSP_USB_FS ? USBFS_UEP_T_TOG : USBHS_UEP_T_TOG_DATA1;
    __asm volatile ("fence iorw, iorw" ::: "memory");
    if (port == BSP_USB_FS) {
        if (in) { USBFSD->UEP0_TX_LEN = length; USBFSD->UEP0_TX_CTRL = ctrl; }
        else USBFSD->UEP0_RX_CTRL = ctrl;
    } else {
        if (in) { USBHSD->UEP0_TX_LEN = length; USBHSD->UEP0_TX_CTRL = ctrl; }
        else USBHSD->UEP0_RX_CTRL = ctrl;
    }
    bsp_unlock(&lock);
    return BSP_OK;
}

static bsp_status_t de_init_port(bsp_usb_port_t port, uint8_t host)
{
    if ((unsigned)port > BSP_USB_HS) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    if (!buffers[port] || host_mode[port] != host || host_busy[port]) {
        bsp_status_t status = host_busy[port] ? BSP_ERR_BUSY : BSP_ERR_STATE;
        bsp_unlock(&lock);
        return status;
    }
    if (port == BSP_USB_FS) USBFSD->INT_EN = 0;
    else USBHSD->INT_EN = 0;
    reset_engine(port);
    if (!host) {
        disable_bulk(port, BSP_USB_BULK_IN1);
        disable_bulk(port, BSP_USB_BULK_OUT2);
    }
    if (port == BSP_USB_FS) USBFSD->UDEV_CTRL = 0;
    else USBHSD->HOST_CTRL = 0;
    long level = bsp_irq_save();
    if (port == BSP_USB_FS) RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBFS, DISABLE);
    if (!buffers[1 - port]) {
        RCC->CFGR2 = (RCC->CFGR2 & ~USB_CLOCK_BITS) | saved_clock_bits;
        RCC_AHBPeriphClockCmd(RCC_AHBPeriph_USBHS, DISABLE);
    }
    bsp_irq_restore(level);
    bsp_ahb_release_pins(&buffers[port], pins[port], 2);
    buffers[port] = NULL;
    host_mode[port] = host_ready[port] = 0;
    bsp_unlock(&lock);
    return BSP_OK;
}

bsp_status_t bsp_usb_de_init(bsp_usb_port_t port)
{
    return de_init_port(port, 0);
}

bsp_status_t bsp_usb_host_de_init(bsp_usb_port_t port)
{
    return de_init_port(port, 1);
}

static bsp_status_t lock_host(bsp_usb_port_t port)
{
    if ((unsigned)port > BSP_USB_HS) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = host_mode[port] ? bsp_try_lock(&host_busy[port]) : BSP_ERR_STATE;
    bsp_unlock(&lock);
    return status;
}

static int attached(bsp_usb_port_t port)
{
    return port == BSP_USB_FS ? (USBFSH->MIS_ST & USBFS_UMS_DEV_ATTACH) != 0 :
                                (USBHSH->MIS_ST & USBHS_UMS_DEV_ATTACH) != 0;
}

bsp_status_t bsp_usb_host_reset(bsp_usb_port_t port, bsp_usb_speed_t *speed)
{
    if (!speed) return BSP_ERR_ARG;
    if (!rt_thread_self() || rt_interrupt_get_nest() || rt_critical_level()) return BSP_ERR_STATE;
    bsp_status_t status = lock_host(port);
    if (status != BSP_OK) return status;
    host_ready[port] = 0;
    if (!attached(port)) { bsp_unlock(&host_busy[port]); return BSP_ERR_STATE; }
    if (port == BSP_USB_FS) {
        USBFSH->DEV_ADDR = 0;
        USBFSH->BASE_CTRL &= ~USBFS_UC_LOW_SPEED;
        USBFSH->HOST_SETUP = 0;
        USBFSH->HOST_CTRL = USBFS_UH_BUS_RESET;
    } else {
        USBHSH->DEV_AD = 0;
        USBHSH->CONTROL = (USBHSH->CONTROL & ~USBHS_UC_SPEED_TYPE) | USBHS_UC_SPEED_HIGH;
        USBHSH->HOST_CTRL = USBHS_UH_PHY_SUSPENDM | USBHS_UH_TX_BUS_RESET;
    }
    /* 先延时至少 10 ms，再释放复位信号，给设备恢复时间。 */
    rt_thread_mdelay(20);
    if (port == BSP_USB_FS) USBFSH->HOST_CTRL &= ~USBFS_UH_BUS_RESET;
    else USBHSH->HOST_CTRL &= ~USBHS_UH_TX_BUS_RESET;
    rt_thread_mdelay(20);
    if (!attached(port)) status = BSP_ERR_STATE;
    else if (port == BSP_USB_FS) {
        *speed = (USBFSH->MIS_ST & USBFS_UMS_DM_LEVEL) ? BSP_USB_LOW_SPEED : BSP_USB_FULL_SPEED;
        if (*speed == BSP_USB_LOW_SPEED) {
            USBFSH->BASE_CTRL |= USBFS_UC_LOW_SPEED;
            USBFSH->HOST_CTRL |= USBFS_UH_LOW_SPEED;
        }
        USBFSH->HOST_CTRL |= USBFS_UH_PORT_EN;
        USBFSH->HOST_SETUP = USBFS_UH_SOF_EN;
        USBFSH->INT_FG = USBFS_UIF_DETECT;
    } else {
        uint8_t negotiated = USBHSH->SPEED_TYPE & USBHS_USB_SPEED_TYPE;
        if (negotiated > USBHS_USB_SPEED_LOW) status = BSP_ERR_IO;
        else {
            *speed = negotiated == USBHS_USB_SPEED_HIGH ? BSP_USB_HIGH_SPEED :
                     negotiated == USBHS_USB_SPEED_LOW ? BSP_USB_LOW_SPEED : BSP_USB_FULL_SPEED;
            uint8_t bits = *speed == BSP_USB_HIGH_SPEED ? USBHS_UC_SPEED_HIGH :
                           *speed == BSP_USB_LOW_SPEED ? USBHS_UC_SPEED_LOW : USBHS_UC_SPEED_FULL;
            USBHSH->CONTROL = (USBHSH->CONTROL & ~USBHS_UC_SPEED_TYPE) | bits;
            USBHSH->HOST_CTRL |= USBHS_UH_SOF_EN;
            USBHSH->INT_FG = USBHS_UIF_DETECT;
        }
    }
    if (status == BSP_OK) { host_speed[port] = *speed; host_ready[port] = 1; }
    bsp_unlock(&host_busy[port]);
    return status;
}

static bsp_status_t run_token(bsp_usb_port_t port, const bsp_usb_request_t *r,
                              bsp_usb_reply_t *reply, uint32_t limit)
{
    uint8_t token = (r->token << 4) | r->endpoint;
    uint8_t toggle = r->data1 ? (port == BSP_USB_FS ? USBFS_UH_T_TOG : USBHS_UH_T_TOG_DATA1) : 0;
    uint16_t tx_length = r->token == BSP_USB_IN ? 0 : r->length;
    if (tx_length) memmove(buffers[port], r->data, tx_length);
    __asm volatile ("fence iorw, iorw" ::: "memory");
    if (port == BSP_USB_FS) {
        USBFSH->DEV_ADDR = r->address;
        USBFSH->HOST_TX_LEN = tx_length;
        USBFSH->HOST_RX_CTRL = USBFSH->HOST_TX_CTRL = toggle;
        USBFSH->HOST_EP_PID = token;
        USBFSH->INT_FG = USBFS_UIF_TRANSFER;
    } else {
        USBHSH->DEV_AD = r->address;
        USBHSH->HOST_TX_LEN = tx_length;
        USBHSH->HOST_RX_CTRL = USBHSH->HOST_TX_CTRL = toggle;
        USBHSH->HOST_EP_PID = token;
        USBHSH->INT_FG = USBHS_UIF_TRANSFER;
    }
    uint8_t flags = 0;
    while (limit--) {
        flags = port == BSP_USB_FS ? USBFSH->INT_FG : USBHSH->INT_FG;
        if (flags & (USBFS_UIF_TRANSFER | USBFS_UIF_DETECT)) break;
    }
    if (port == BSP_USB_FS) USBFSH->HOST_EP_PID = 0;
    else USBHSH->HOST_EP_PID = 0;
    if (flags & USBFS_UIF_DETECT) {
        host_ready[port] = 0;
        return BSP_ERR_STATE;
    }
    if (!(flags & USBFS_UIF_TRANSFER)) return BSP_ERR_TIMEOUT;
    uint8_t state = port == BSP_USB_FS ? USBFSH->INT_ST : USBHSH->INT_ST;
    reply->pid = state & USBFS_UIS_H_RES_MASK;
    if (reply->pid == USB_PID_NAK) return BSP_ERR_BUSY;
    if (!reply->pid) return BSP_ERR_TIMEOUT;
    if (r->token != BSP_USB_IN)
        return reply->pid == USB_PID_ACK || (port == BSP_USB_HS && reply->pid == USB_PID_NYET) ?
               BSP_OK : BSP_ERR_IO;
    if (!(state & USBFS_UIS_TOG_OK) || reply->pid != (r->data1 ? USB_PID_DATA1 : USB_PID_DATA0))
        return BSP_ERR_IO;
    reply->received = port == BSP_USB_FS ? USBFSH->RX_LEN : USBHSH->RX_LEN;
    if (reply->received > r->length) return BSP_ERR_ARG;
    __asm volatile ("fence iorw, iorw" ::: "memory");
    if (reply->received) memmove(r->data, buffers[port], reply->received);
    return BSP_OK;
}

bsp_status_t bsp_usb_host_transfer(bsp_usb_port_t port, const bsp_usb_request_t *r,
                                   bsp_usb_reply_t *reply, uint32_t poll_limit)
{
    if (!r || !reply || !poll_limit || r->address > 127 || r->endpoint > 15 || r->data1 > 1 ||
        (r->length && !r->data) || (r->token != BSP_USB_OUT && r->token != BSP_USB_IN && r->token != BSP_USB_SETUP) ||
        (r->token == BSP_USB_SETUP && (r->length != 8 || r->data1))) return BSP_ERR_ARG;
    *reply = (bsp_usb_reply_t){0};
    bsp_status_t status = lock_host(port);
    if (status != BSP_OK) return status;
    uint16_t max_packet = host_speed[port] == BSP_USB_HIGH_SPEED ? 512 :
                          host_speed[port] == BSP_USB_LOW_SPEED ? 8 : 64;
    uint8_t flags = port == BSP_USB_FS ? USBFSH->INT_FG : USBHSH->INT_FG;
    if (!host_ready[port] || !attached(port) || (flags & USBFS_UIF_DETECT)) {
        host_ready[port] = 0;
        status = BSP_ERR_STATE;
    }
    else if (r->length > max_packet) status = BSP_ERR_ARG;
    else status = run_token(port, r, reply, poll_limit);
    bsp_unlock(&host_busy[port]);
    return status;
}
