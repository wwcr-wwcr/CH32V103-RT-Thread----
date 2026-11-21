#include "bsp_can.h"
#include "bsp_bus_internal.h"

/* 默认/重映射各一对 RX/TX。CAN1 的路由值 1 为保留编码。 */
static const bsp_pin_t pins[][2] = {
    {{GPIOA, GPIO_Pin_11}, {GPIOA, GPIO_Pin_12}},
    {{GPIOB, GPIO_Pin_8}, {GPIOB, GPIO_Pin_9}},
    {{GPIOD, GPIO_Pin_0}, {GPIOD, GPIO_Pin_1}},
    {{GPIOB, GPIO_Pin_12}, {GPIOB, GPIO_Pin_13}},
    {{GPIOB, GPIO_Pin_5}, {GPIOB, GPIO_Pin_6}}
};

static struct {
    volatile uint8_t lock;
    uint8_t active, pins;
    uint32_t previous;
} state[2];
/* 仅串行化共享滤波器和时钟的生命周期。独立实例可并发收发。 */
static volatile uint8_t setup_lock;

static int index_of(CAN_TypeDef *bus)
{
    if (bus == CAN1) return 0;
    if (bus == CAN2) return 1;
    return -1;
}

static int pin_index(int i, unsigned route)
{
    if (i == 0 && route == 0) return 0;
    if (i == 0 && (route == 2 || route == 3)) return (int)route - 1;
    if (i == 1 && route <= 1) return (int)route + 3;
    return -1;
}

/* 优先 16 个时间份额、87.5% 采样点，再寻找精确匹配当前 PCLK1 的组合。 */
static bsp_status_t set_timing(CAN_InitTypeDef *cfg, uint32_t bitrate)
{
    RCC_ClocksTypeDef clocks;
    RCC_GetClocksFreq(&clocks);
    if (!bitrate || bitrate > 1000000U || clocks.PCLK1_Frequency % bitrate) return BSP_ERR_ARG;
    uint32_t clocks_per_bit = clocks.PCLK1_Frequency / bitrate;
    static const uint8_t quanta[] = {16, 8, 12, 18, 20, 24, 10, 14, 15, 17, 19, 21,
                                     22, 23, 25, 9, 11, 13, 7, 6, 5, 4};
    for (unsigned k = 0; k < sizeof quanta; ++k) {
        unsigned tq = quanta[k];
        unsigned prescale = clocks_per_bit / tq;
        if (clocks_per_bit % tq || prescale < 1 || prescale > 1024) continue;
        unsigned bs2 = (tq + 4U) / 8U;
        if (tq > 17U + bs2) bs2 = tq - 17U;
        if (bs2 < 1) bs2 = 1;
        cfg->CAN_SJW = CAN_SJW_1tq;
        cfg->CAN_BS1 = tq - bs2 - 2U;
        cfg->CAN_BS2 = bs2 - 1U;
        cfg->CAN_Prescaler = prescale;
        return BSP_OK;
    }
    return BSP_ERR_ARG;
}

static void prepare_filters(void)
{
    CAN_FilterInitTypeDef filter = {0};
    /* 两个实例的开放滤波器一次完成，第二个启动时无需暂停活动总线的接收。 */
    CAN_SlaveStartBank(1);
    filter.CAN_FilterScale = CAN_FilterScale_32bit;
    filter.CAN_FilterMode = CAN_FilterMode_IdMask;
    filter.CAN_FilterFIFOAssignment = CAN_Filter_FIFO0;
    filter.CAN_FilterActivation = ENABLE;
    for (unsigned n = 0; n < 2; ++n) {
        filter.CAN_FilterNumber = n;
        CAN_FilterInit(&filter);
    }
}

static void release(int i, CAN_TypeDef *bus)
{
    long level = bsp_irq_save();
    int peer_active = state[1 - i].active;
    /* CAN2 仍在运行时，复位 CAN1 会清除它使用的接收过滤器。 */
    if (i == 1) {
        CAN_DeInit(bus);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN2, DISABLE);
    }
    if (!peer_active) {
        CAN_DeInit(CAN1);
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1, DISABLE);
    }
    bus_set_route(&AFIO->PCFR1, i ? AFIO_PCFR1_CAN2_REMAP : AFIO_PCFR1_CAN_REMAP, state[i].previous);
    bsp_irq_restore(level);
    bus_release_pins(&state[i], pins[state[i].pins], 2);
    state[i].active = 0;
}

bsp_status_t bsp_can_init(CAN_TypeDef *bus, unsigned route, uint32_t bitrate, unsigned mode)
{
    int i = index_of(bus);
    int pin = pin_index(i, route);
    CAN_InitTypeDef cfg;
    if (i < 0 || pin < 0 || mode > CAN_Mode_Silent_LoopBack) return BSP_ERR_ARG;
    CAN_StructInit(&cfg);
    bsp_status_t rc = set_timing(&cfg, bitrate);
    if (rc != BSP_OK) return rc;
    if (bsp_try_lock(&setup_lock) != BSP_OK) return BSP_ERR_BUSY;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) { bsp_unlock(&setup_lock); return BSP_ERR_BUSY; }
    unsigned revision = (DBGMCU_GetCHIPID() >> 4) & 15U;
    int peer_active = state[1 - i].active;
    if (state[i].active || (peer_active && revision >= 4 && revision <= 7)) {
        rc = BSP_ERR_BUSY;
        goto done;
    }
    rc = bsp_pins_claim(&state[i], pins[pin], 2);
    if (rc != BSP_OK) goto done;
    state[i].pins = pin;
    long level = bsp_irq_save();
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_CAN1 | (i ? RCC_APB1Periph_CAN2 : 0), ENABLE);
    if (!peer_active) CAN_DeInit(CAN1);
    if (i == 1) CAN_DeInit(CAN2);
    uint32_t mask = i ? AFIO_PCFR1_CAN2_REMAP : AFIO_PCFR1_CAN_REMAP;
    uint32_t value = i ? (route ? mask : 0) : (uint32_t)route << 13;
    state[i].previous = bus_set_route(&AFIO->PCFR1, mask, value);
    bsp_irq_restore(level);
    bsp_pin_config(pins[pin][0], GPIO_Mode_IPU);
    bsp_pin_config(pins[pin][1], GPIO_Mode_AF_PP);
    cfg.CAN_Mode = mode;
    cfg.CAN_ABOM = ENABLE;
    cfg.CAN_NART = ENABLE; /* 不让一次发送在总线上无限重发。 */
    if (CAN_Init(bus, &cfg) != CAN_InitStatus_Success) {
        if (i == 0 && peer_active) bus->CTLR |= CAN_CTLR_INRQ;
        release(i, bus);
        rc = BSP_ERR_TIMEOUT;
    } else {
        if (!peer_active) prepare_filters();
        state[i].active = 1;
    }
done:
    bsp_unlock(&state[i].lock);
    bsp_unlock(&setup_lock);
    return rc;
}

bsp_status_t bsp_can_send(CAN_TypeDef *bus, const CanTxMsg *frame)
{
    int i = index_of(bus);
    if (i < 0 || !frame || frame->DLC > 8 ||
        (frame->RTR != CAN_RTR_Data && frame->RTR != CAN_RTR_Remote) ||
        (frame->IDE != CAN_Id_Standard && frame->IDE != CAN_Id_Extended) ||
        (frame->IDE == CAN_Id_Standard ? frame->StdId > 0x7ffU : frame->ExtId > 0x1fffffffU))
        return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = BSP_ERR_STATE;
    if (state[i].active) {
        CanTxMsg message = *frame;
        unsigned mailbox = CAN_Transmit(bus, &message);
        rc = BSP_ERR_BUSY;
        if (mailbox != CAN_TxStatus_NoMailBox) {
            uint32_t budget = BSP_BUS_POLL_LIMIT;
            unsigned status;
            do {
                status = CAN_TransmitStatus(bus, mailbox);
            } while (status == CAN_TxStatus_Pending && --budget);
            if (status == CAN_TxStatus_Pending) {
                CAN_CancelTransmit(bus, mailbox);
                rc = BSP_ERR_TIMEOUT;
            } else rc = status == CAN_TxStatus_Ok ? BSP_OK : BSP_ERR_IO;
        }
    }
    bsp_unlock(&state[i].lock);
    return rc;
}

bsp_status_t bsp_can_receive(CAN_TypeDef *bus, CanRxMsg *frame)
{
    int i = index_of(bus);
    if (i < 0 || !frame) return BSP_ERR_ARG;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t rc = BSP_ERR_STATE;
    if (state[i].active) {
        if (bus->RFIFO0 & CAN_RFIFO0_FOVR0) {
            bus->RFIFO0 = CAN_RFIFO0_FOVR0; /* W1C，保留待处理帧。 */
            rc = BSP_ERR_IO;
        } else if (CAN_MessagePending(bus, CAN_FIFO0)) {
            CAN_Receive(bus, CAN_FIFO0, frame);
            rc = frame->DLC <= 8 ? BSP_OK : BSP_ERR_IO;
        } else rc = BSP_ERR_BUSY;
    }
    bsp_unlock(&state[i].lock);
    return rc;
}

bsp_status_t bsp_can_de_init(CAN_TypeDef *bus)
{
    int i = index_of(bus);
    if (i < 0) return BSP_ERR_ARG;
    if (bsp_try_lock(&setup_lock) != BSP_OK) return BSP_ERR_BUSY;
    if (bsp_try_lock(&state[i].lock) != BSP_OK) { bsp_unlock(&setup_lock); return BSP_ERR_BUSY; }
    bsp_status_t rc = BSP_OK;
    if (!state[i].active) rc = BSP_ERR_STATE;
    else if (i == 0 && state[1].active &&
             CAN_OperatingModeRequest(bus, CAN_OperatingMode_Initialization) != CAN_ModeStatus_Success)
        rc = BSP_ERR_TIMEOUT;
    else {
        if (i == 0 && state[1].active) {
            for (unsigned n = CAN_MessagePending(bus, CAN_FIFO0); n > 0; --n)
                CAN_FIFORelease(bus, CAN_FIFO0);
        }
        release(i, bus);
    }
    bsp_unlock(&state[i].lock);
    bsp_unlock(&setup_lock);
    return rc;
}
