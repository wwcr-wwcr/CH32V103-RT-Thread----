#ifndef BSP_RNG_H
#define BSP_RNG_H
#include "bsp_types.h"
/* 开启硬件随机数发生器。 */
bsp_status_t bsp_rng_init(void);
/* 向 dst 写入 count 个 32 位随机字，超时或发生种子/时钟错误时仅前缀有效。
 * written 可传 NULL，传入时可获知有效字数。 */
bsp_status_t bsp_rng_read(uint32_t *dst, size_t count, size_t *written);
/* 关闭发生器并释放时钟。 */
bsp_status_t bsp_rng_de_init(void);
#endif
