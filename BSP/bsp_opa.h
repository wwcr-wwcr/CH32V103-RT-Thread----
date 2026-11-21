#ifndef BSP_OPA_H
#define BSP_OPA_H
#include "bsp_types.h"
#include "ch32v30x_opa.h"

/* 用途：连接并启用一个片上运放。参数：opa 为 OPA1~OPA4，正输入 CHP0/CHP1，
 * 负输入 CHN0/CHN1，输出 OUT_IO_OUT0/OUT_IO_OUT1。
 * 注意：闭环需外部反馈；本板 OPA3 使用 CHP1(PC5)、CHN1(PC2)、OUT0(PA1)。
 * ADC 可以同时采样其输出；输出冲突或碰到系统保留引脚时返回 BUSY。 */
bsp_status_t bsp_opa_init(OPA_Num_TypeDef opa, OPA_PSEL_TypeDef positive,
                          OPA_NSEL_TypeDef negative, OPA_Mode_TypeDef output);
/* 用途：关闭运放并释放其三个引脚。参数：opa 为 OPA1~OPA4。注意：其他运放继续工作。 */
bsp_status_t bsp_opa_de_init(OPA_Num_TypeDef opa);
#endif
