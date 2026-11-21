#include "bsp_sdio.h"
#include "bsp_ahb_internal.h"

static const bsp_pin_t pins[] = {
    {GPIOC, GPIO_Pin_8}, {GPIOC, GPIO_Pin_12}, {GPIOD, GPIO_Pin_2},
    {GPIOC, GPIO_Pin_9}, {GPIOC, GPIO_Pin_10}, {GPIOC, GPIO_Pin_11}
};
static uint8_t wires_active;
static volatile uint8_t lock;
#define CMD_FLAGS (SDIO_FLAG_CMDSENT | SDIO_FLAG_CMDREND | SDIO_FLAG_CTIMEOUT | SDIO_FLAG_CCRCFAIL)
#define DATA_ERRORS (SDIO_FLAG_DCRCFAIL | SDIO_FLAG_DTIMEOUT | SDIO_FLAG_TXUNDERR | SDIO_FLAG_RXOVERR | SDIO_FLAG_STBITERR)
#define DATA_FLAGS (DATA_ERRORS | SDIO_FLAG_DATAEND | SDIO_FLAG_DBCKEND)

static void configure(uint8_t width, uint8_t divider)
{
    SDIO_InitTypeDef init;
    SDIO_StructInit(&init);
    init.SDIO_BusWide = width == 4 ? SDIO_BusWide_4b : SDIO_BusWide_1b;
    init.SDIO_ClockDiv = divider;
    SDIO_ClockCmd(DISABLE);
    SDIO_Init(&init);
    SDIO_ClockCmd(ENABLE);
}

bsp_status_t bsp_sdio_init(uint8_t wires, uint8_t divider)
{
    if (wires != 1 && wires != 4) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    size_t count = wires + 2;
    bsp_status_t status = wires_active || (RCC->AHBPCENR & RCC_AHBPeriph_SDIO) ? BSP_ERR_BUSY :
                          bsp_pins_claim(&wires_active, pins, count);
    if (status == BSP_OK) {
        for (size_t i = 0; i < count; ++i) bsp_pin_config(pins[i], GPIO_Mode_AF_PP);
        bsp_ahb_clock(RCC_AHBPeriph_SDIO, ENABLE);
        SDIO_DeInit();
        SDIO_SetPowerState(SDIO_PowerState_ON);
        configure(1, divider);
        wires_active = wires;
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_sdio_configure(uint8_t width, uint8_t divider)
{
    if (width != 1 && width != 4) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (width <= wires_active) {
        configure(width, divider);
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}

static bsp_status_t command(uint8_t cmd, uint32_t arg, bsp_sdio_response_t kind,
                            uint32_t *response, uint32_t limit)
{
    SDIO_CmdInitTypeDef request = {0};
    request.SDIO_Argument = arg;
    request.SDIO_CmdIndex = cmd;
    request.SDIO_Response = kind == BSP_SDIO_LONG ? SDIO_Response_Long :
                            kind == BSP_SDIO_NO_RESPONSE ? SDIO_Response_No : SDIO_Response_Short;
    request.SDIO_CPSM = SDIO_CPSM_Enable;
    SDIO_ClearFlag(CMD_FLAGS);
    SDIO_SendCommand(&request);
    bsp_status_t status = BSP_ERR_TIMEOUT;
    while (limit--) {
        uint32_t flags = SDIO->STA;
        if (flags & SDIO_FLAG_CTIMEOUT) break;
        if ((flags & SDIO_FLAG_CCRCFAIL) && kind != BSP_SDIO_SHORT_NO_CRC) {
            status = BSP_ERR_IO;
            break;
        }
        if (flags & (SDIO_FLAG_CMDSENT | SDIO_FLAG_CMDREND | SDIO_FLAG_CCRCFAIL)) {
            if (kind != BSP_SDIO_NO_RESPONSE) response[0] = SDIO->RESP1;
            if (kind == BSP_SDIO_LONG) {
                response[1] = SDIO->RESP2;
                response[2] = SDIO->RESP3;
                response[3] = SDIO->RESP4;
            }
            status = BSP_OK;
            break;
        }
    }
    SDIO->CMD = 0;
    SDIO_ClearFlag(CMD_FLAGS);
    return status;
}

bsp_status_t bsp_sdio_command(uint8_t cmd, uint32_t arg, bsp_sdio_response_t kind,
                              uint32_t *response, uint32_t poll_limit)
{
    if (cmd > 63 || (unsigned)kind > BSP_SDIO_LONG || !poll_limit ||
        (kind != BSP_SDIO_NO_RESPONSE && !response)) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = wires_active ? command(cmd, arg, kind, response, poll_limit) : BSP_ERR_STATE;
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_sdio_block(uint8_t write, uint32_t arg, uint32_t words[128], uint32_t poll_limit)
{
    if (write > 1 || !words || ((uintptr_t)words & 3u) || !poll_limit) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (wires_active) {
        uint32_t response;
        size_t transferred = 0;
        SDIO_DataInitTypeDef data = {0};
        data.SDIO_DataTimeOut = UINT32_MAX;
        data.SDIO_DataLength = 512;
        data.SDIO_DataBlockSize = SDIO_DataBlockSize_512b;
        data.SDIO_TransferDir = write ? SDIO_TransferDir_ToCard : SDIO_TransferDir_ToSDIO;
        data.SDIO_DPSM = SDIO_DPSM_Enable;
        SDIO_ClearFlag(DATA_FLAGS);
        if (!write) SDIO_DataConfig(&data);
        status = command(write ? 24 : 17, arg, BSP_SDIO_SHORT, &response, poll_limit);
        if (status == BSP_OK && (response & 0xFDFFE008u)) status = BSP_ERR_IO;
        if (status == BSP_OK) {
            if (write) SDIO_DataConfig(&data);
            status = BSP_ERR_TIMEOUT;
            while (poll_limit--) {
                uint32_t flags = SDIO->STA;
                if (flags & DATA_ERRORS) {
                    status = (flags & SDIO_FLAG_DTIMEOUT) ? BSP_ERR_TIMEOUT : BSP_ERR_IO;
                    break;
                }
                if (!write && (flags & SDIO_FLAG_RXDAVL) && transferred < 128)
                    words[transferred++] = SDIO->FIFO;
                else if (write && (flags & SDIO_FLAG_TXFIFOHE) && transferred < 128)
                    for (unsigned i = 0; i < 8; ++i) SDIO->FIFO = words[transferred++];
                else if (flags & SDIO_FLAG_DATAEND) {
                    status = transferred == 128 ? BSP_OK : BSP_ERR_IO;
                    break;
                }
            }
        }
        SDIO->DCTRL = 0;
        SDIO_ClearFlag(DATA_FLAGS);
        /* 错误中止后 FIFO 可能遗留上次数据。保持总线配置，重置控制器后再允许重试。 */
        if (status != BSP_OK) {
            uint32_t clock = SDIO->CLKCR;
            SDIO_DeInit();
            SDIO_SetPowerState(SDIO_PowerState_ON);
            SDIO->CLKCR = clock;
        }
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_sdio_de_init(void)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (wires_active) {
        SDIO_DeInit();
        bsp_ahb_clock(RCC_AHBPeriph_SDIO, DISABLE);
        bsp_ahb_release_pins(&wires_active, pins, wires_active + 2);
        wires_active = 0;
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}
