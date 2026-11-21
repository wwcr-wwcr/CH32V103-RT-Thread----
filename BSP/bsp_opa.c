#include "bsp_opa.h"
#include "bsp_analog_pin.h"

/* 数据手册表 3-4。每组依次为正输入、负输入、输出，各有 0/1 两种路由。 */
static const bsp_pin_t routes[4][3][2] = {
    {{{GPIOB, GPIO_Pin_15}, {GPIOB, GPIO_Pin_0}}, {{GPIOB, GPIO_Pin_11}, {GPIOA, GPIO_Pin_6}}, {{GPIOA, GPIO_Pin_3}, {GPIOE, GPIO_Pin_15}}},
    {{{GPIOB, GPIO_Pin_14}, {GPIOA, GPIO_Pin_7}}, {{GPIOB, GPIO_Pin_10}, {GPIOA, GPIO_Pin_5}}, {{GPIOA, GPIO_Pin_2}, {GPIOE, GPIO_Pin_14}}},
    {{{GPIOB, GPIO_Pin_13}, {GPIOC, GPIO_Pin_5}}, {{GPIOB, GPIO_Pin_2},  {GPIOC, GPIO_Pin_2}}, {{GPIOA, GPIO_Pin_1}, {GPIOE, GPIO_Pin_7}}},
    {{{GPIOB, GPIO_Pin_12}, {GPIOC, GPIO_Pin_4}}, {{GPIOB, GPIO_Pin_1},  {GPIOC, GPIO_Pin_3}}, {{GPIOA, GPIO_Pin_0}, {GPIOE, GPIO_Pin_8}}}
};

typedef struct {
    uint8_t active;
    uint8_t selection[3];
} opa_state_t;
static opa_state_t states[4];

static void release_pins(OPA_Num_TypeDef opa, unsigned count)
{
    while (count) {
        unsigned i = --count;
        bsp_pin_t pin = routes[opa][i][states[opa].selection[i]];
        bsp_analog_pin_release(pin, i == 2 ? BSP_ANALOG_OUTPUT : BSP_ANALOG_INPUT);
    }
}

bsp_status_t bsp_opa_init(OPA_Num_TypeDef opa, OPA_PSEL_TypeDef positive,
                          OPA_NSEL_TypeDef negative, OPA_Mode_TypeDef output)
{
    if ((unsigned)opa > OPA4 || (unsigned)positive > CHP1 ||
        (unsigned)negative > CHN1 || (unsigned)output > OUT_IO_OUT1) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    opa_state_t *state = &states[opa];
    bsp_status_t result = BSP_ERR_BUSY;
    if (!state->active) {
        state->selection[0] = positive;
        state->selection[1] = negative;
        state->selection[2] = output;
        unsigned i;
        for (i = 0; i < 3; i++) {
            bsp_pin_t pin = routes[opa][i][state->selection[i]];
            result = bsp_analog_pin_claim(pin, i == 2 ? BSP_ANALOG_OUTPUT : BSP_ANALOG_INPUT);
            if (result != BSP_OK) break;
        }
        if (result == BSP_OK) {
            OPA_InitTypeDef init = {opa, positive, negative, output};
            OPA_Init(&init);
            OPA_Cmd(opa, ENABLE);
            state->active = 1;
        } else release_pins(opa, i);
    }
    bsp_irq_restore(level);
    return result;
}

bsp_status_t bsp_opa_de_init(OPA_Num_TypeDef opa)
{
    if ((unsigned)opa > OPA4) return BSP_ERR_ARG;
    long level = bsp_irq_save();
    bsp_status_t result = BSP_ERR_STATE;
    if (states[opa].active) {
        OPA_Cmd(opa, DISABLE);
        release_pins(opa, 3);
        states[opa].active = 0;
        result = BSP_OK;
    }
    bsp_irq_restore(level);
    return result;
}
