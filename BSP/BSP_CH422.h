/*数码管底层函数*/
/*======== BSP_CH422.h =================*/

#ifndef __BSP_CH422_H
#define __BSP_CH422_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ch32v30x_conf.h"

/* 引脚：SCL = PE0，SDA = PE1 */
#define BSP_CH422_SCL_PORT      GPIOE
#define BSP_CH422_SCL_PIN       GPIO_Pin_0

#define BSP_CH422_SDA_PORT      GPIOE
#define BSP_CH422_SDA_PIN       GPIO_Pin_1

/* 电平原语：只表示"这根线现在是什么电平"，不含任何时序 */
#define BSP_CH422_SCL_H()       GPIO_SetBits(BSP_CH422_SCL_PORT, BSP_CH422_SCL_PIN)
#define BSP_CH422_SCL_L()       GPIO_ResetBits(BSP_CH422_SCL_PORT, BSP_CH422_SCL_PIN)

#define BSP_CH422_SDA_H()       GPIO_SetBits(BSP_CH422_SDA_PORT, BSP_CH422_SDA_PIN)
#define BSP_CH422_SDA_L()       GPIO_ResetBits(BSP_CH422_SDA_PORT, BSP_CH422_SDA_PIN)

#define BSP_CH422_SDA_READ()    GPIO_ReadInputDataBit(BSP_CH422_SDA_PORT, BSP_CH422_SDA_PIN)

/* 生命周期 */
int BSP_CH422_Init(void);
int BSP_CH422_DeInit(void);

/* 引脚方向：读 SDA 前切输入，读完切回输出 */
void BSP_CH422_SDA_Input(void);
void BSP_CH422_SDA_Output(void);

/* 平台相关延时：迁到 RT-Thread 只改这一处 */
void BSP_CH422_DelayUs(uint16_t us);

#ifdef __cplusplus
}
#endif

#endif