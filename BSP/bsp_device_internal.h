#ifndef BSP_DEVICE_INTERNAL_H
#define BSP_DEVICE_INTERNAL_H
#include "bsp_resource.h"

/* 单实例外设的两种进入条件。调用者持锁期间才修改 active。 */
typedef struct { volatile uint8_t lock; uint8_t active; } bsp_device_t;
static inline bsp_status_t bsp_device_enter(bsp_device_t *dev, int need_active)
{
    bsp_status_t result = bsp_try_lock(&dev->lock);
    if (result != BSP_OK) return result;
    if (dev->active == need_active) return BSP_OK;
    bsp_unlock(&dev->lock);
    return need_active ? BSP_ERR_STATE : BSP_ERR_BUSY;
}
static inline void bsp_device_leave(bsp_device_t *dev, int active)
{
    dev->active = (uint8_t)active;
    bsp_unlock(&dev->lock);
}

/* RTC/BKP/PVD 对 APB1 时钟使用相同引用计数；备份域写使能单独互斥。 */
void bsp_power_clock_get(int backup);
void bsp_power_clock_put(int backup);
bsp_status_t bsp_backup_write_begin(void);
void bsp_backup_write_end(void);
#endif
