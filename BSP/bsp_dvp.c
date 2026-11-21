#include "bsp_dvp.h"
#include "bsp_ahb_internal.h"

static const bsp_pin_t pins[] = {
    {GPIOA, GPIO_Pin_9}, {GPIOA, GPIO_Pin_10}, {GPIOC, GPIO_Pin_8}, {GPIOC, GPIO_Pin_9},
    {GPIOC, GPIO_Pin_11}, {GPIOB, GPIO_Pin_6}, {GPIOB, GPIO_Pin_8}, {GPIOB, GPIO_Pin_9},
    {GPIOA, GPIO_Pin_6}, {GPIOA, GPIO_Pin_4}, {GPIOA, GPIO_Pin_5}
};
#define PIN_COUNT (sizeof(pins) / sizeof(pins[0]))
static uint32_t frame_words;
static volatile uint8_t lock;

bsp_status_t bsp_dvp_init(uint16_t row_bytes, uint16_t rows, uint8_t polarity)
{
    if (!row_bytes || (row_bytes & 3u) || !rows ||
        (polarity & ~(RB_DVP_P_POLAR | RB_DVP_H_POLAR | RB_DVP_V_POLAR))) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = frame_words || (RCC->AHBPCENR & RCC_AHBPeriph_DVP) ? BSP_ERR_BUSY :
                          bsp_pins_claim(&frame_words, pins, PIN_COUNT);
    if (status == BSP_OK) {
        for (size_t i = 0; i < PIN_COUNT; ++i) bsp_pin_config(pins[i], GPIO_Mode_IN_FLOATING);
        bsp_ahb_clock(RCC_AHBPeriph_DVP, ENABLE);
        DVP->CR0 = polarity | RB_DVP_D8_MOD;
        DVP->CR1 = RB_DVP_ALL_CLR | RB_DVP_RCV_CLR;
        DVP->CR1 = 0;
        DVP->IER = 0;
        DVP->ROW_NUM = rows;
        DVP->COL_NUM = row_bytes;
        frame_words = (uint32_t)(row_bytes / 4u) * rows;
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_dvp_capture(uint32_t *dst, size_t capacity, uint32_t poll_limit)
{
    if (!dst || ((uintptr_t)dst & 3u) || !poll_limit) return BSP_ERR_ARG;
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (frame_words && capacity < frame_words) status = BSP_ERR_ARG;
    else if (frame_words) {
        uint32_t n = 0;
        DVP->CR1 = RB_DVP_ALL_CLR | RB_DVP_RCV_CLR;
        DVP->CR1 = RB_DVP_CM; /* 单帧模式，禁止 DMA，CPU 读取数据寄存器。 */
        DVP->CR0 |= RB_DVP_ENABLE;
        status = BSP_ERR_TIMEOUT;
        while (poll_limit--) {
            uint8_t state = DVP->STATUS;
            if ((state & RB_DVP_FIFO_OV) || (DVP->IFR & RB_DVP_IF_FIFO_OV)) {
                status = BSP_ERR_IO;
                break;
            }
            if ((state & RB_DVP_FIFO_RDY) && n < frame_words) {
                dst[n++] = DVP->DR;
                if (n == frame_words) { status = BSP_OK; break; }
            }
        }
        DVP->CR0 &= ~RB_DVP_ENABLE;
    }
    bsp_unlock(&lock);
    return status;
}

bsp_status_t bsp_dvp_de_init(void)
{
    if (bsp_try_lock(&lock) != BSP_OK) return BSP_ERR_BUSY;
    bsp_status_t status = BSP_ERR_STATE;
    if (frame_words) {
        DVP->CR0 = 0;
        DVP->IER = 0;
        DVP->CR1 = RB_DVP_ALL_CLR | RB_DVP_RCV_CLR;
        bsp_ahb_clock(RCC_AHBPeriph_DVP, DISABLE);
        bsp_ahb_release_pins(&frame_words, pins, PIN_COUNT);
        frame_words = 0;
        status = BSP_OK;
    }
    bsp_unlock(&lock);
    return status;
}
