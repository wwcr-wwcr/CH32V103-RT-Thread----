#include "BSP_CH422.h"
#include "debug.h"

static uint8_t s_inited = 0;

void BSP_CH422_DelayUs(uint16_t us)
{
    Delay_Us((uint32_t)us);
}

void BSP_CH422_SDA_Input(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.GPIO_Pin   = BSP_CH422_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_CH422_SDA_PORT, &gpio);
}

void BSP_CH422_SDA_Output(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.GPIO_Pin   = BSP_CH422_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_CH422_SDA_PORT, &gpio);
}

int BSP_CH422_Init(void)
{
    GPIO_InitTypeDef gpio = {0};

    if (s_inited != 0)
    {
        return 0;                       /* 幂等：重复调用直接成功 */
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE, ENABLE);

    /* SCL：只由主机输出 → 推挽 */
    gpio.GPIO_Pin   = BSP_CH422_SCL_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_CH422_SCL_PORT, &gpio);

    /* SDA：双向 → 开漏，高电平靠释放后由 CH422 内置上拉给出 */
    gpio.GPIO_Pin   = BSP_CH422_SDA_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_OD;
    GPIO_Init(BSP_CH422_SDA_PORT, &gpio);

    /* 总线空闲：两根都高，避免上电被误判成 START */
    BSP_CH422_SCL_H();
    BSP_CH422_SDA_H();

    s_inited = 1;
    return 0;
}

int BSP_CH422_DeInit(void)
{
    GPIO_InitTypeDef gpio = {0};

    if (s_inited == 0)
    {
        return 0;
    }

    /* 1. 先把总线放回空闲，不能留低电平 */
    BSP_CH422_SCL_H();
    BSP_CH422_SDA_H();

    /* 2. 引脚还给复位默认：浮空输入 */
    gpio.GPIO_Pin   = BSP_CH422_SCL_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_IN_FLOATING;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BSP_CH422_SCL_PORT, &gpio);

    gpio.GPIO_Pin   = BSP_CH422_SDA_PIN;
    GPIO_Init(BSP_CH422_SDA_PORT, &gpio);

    /* 3. 不关 GPIOE 时钟：PE2~PE5 是按键，关了整个 E 口都停 */

    s_inited = 0;
    return 0;
}