#include "bsp_dac.h"
#include "bsp_analog_pin.h"
#include "bsp_dac_playback_internal.h"

static uint8_t active;
static uint8_t streaming;
static const bsp_pin_t pins[] = {{GPIOA, GPIO_Pin_4}, {GPIOA, GPIO_Pin_5}};

static void write_code(uint8_t channel, uint16_t code)
{
    if (channel == 1) DAC_SetChannel1Data(DAC_Align_12b_R, code);
    else DAC_SetChannel2Data(DAC_Align_12b_R, code);
}

bsp_status_t bsp_dac_init(uint8_t channel)
{
    DAC_InitTypeDef init;
    if (channel < 1 || channel > 2) return BSP_ERR_ARG;
    uint32_t hw = channel == 1 ? DAC_Channel_1 : DAC_Channel_2;
    uint8_t bit = 1U << (channel - 1);
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_BUSY;
    if (!(active & bit)) {
        result = bsp_analog_pin_claim(pins[channel - 1], BSP_ANALOG_OUTPUT);
        if (result == BSP_OK) {
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, ENABLE);
            DAC_StructInit(&init);
            DAC_Init(hw, &init);
            write_code(channel, 0);
            DAC_Cmd(hw, ENABLE);
            active |= bit;
        }
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_dac_write(uint8_t channel, uint16_t code)
{
    if (channel < 1 || channel > 2 || code > 4095) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (streaming & (1U << (channel - 1))) result = BSP_ERR_BUSY;
    else if (active & (1U << (channel - 1))) {
        write_code(channel, code);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_dac_de_init(uint8_t channel)
{
    if (channel < 1 || channel > 2) return BSP_ERR_ARG;
    uint8_t bit = 1U << (channel - 1);
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (streaming & bit) result = BSP_ERR_BUSY;
    else if (active & bit) {
        DAC_Cmd(channel == 1 ? DAC_Channel_1 : DAC_Channel_2, DISABLE);
        active &= ~bit;
        if (!active) {
            DAC_DeInit();
            RCC_APB1PeriphClockCmd(RCC_APB1Periph_DAC, DISABLE);
        }
        bsp_analog_pin_release(pins[channel - 1], BSP_ANALOG_OUTPUT);
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_dac_hold_output(uint8_t channel)
{
    uint8_t bit = 1U << (channel - 1U);
    if (!(active & bit)) return BSP_ERR_STATE;
    if (streaming & bit) return BSP_ERR_BUSY;
    streaming |= bit;
    return BSP_OK;
}

void bsp_dac_release_output(uint8_t channel, uint16_t idle)
{
    write_code(channel, idle);
    streaming &= ~(1U << (channel - 1U));
}
