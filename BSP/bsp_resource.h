#ifndef BSP_RESOURCE_H
#define BSP_RESOURCE_H

#include "bsp_types.h"

/* 端口 A..E 和单比特掩码映射到数组下标，非法输入返回 -1。 */
int bsp_port_index(GPIO_TypeDef *port);
int bsp_pin_index(uint16_t bit);

/* owner 取静态实例或通道对象的地址。全组原子申请，重复申请也报 BUSY。release 仅释放所属引脚。 */
bsp_status_t bsp_pins_claim(const void *owner, const bsp_pin_t *pins, size_t count);
void bsp_pins_release(const void *owner, const bsp_pin_t *pins, size_t count);
/* 配置前必须持有该引脚。config 打开端口时钟，release 前可用 floating 恢复输入。 */
void bsp_pin_config(bsp_pin_t pin, GPIOMode_TypeDef mode);
void bsp_pin_floating(bsp_pin_t pin);
/* ETHMAC 启用时 SPI3/I2S3 引脚有额外限制，本板对这两种用途互斥分配。 */
extern volatile uint8_t bsp_eth_spi3_lock;
/* 非阻塞实例锁。静态 uint8_t 初值为 0。长操作期间不屏蔽中断。 */
bsp_status_t bsp_try_lock(volatile uint8_t *lock);
void bsp_unlock(volatile uint8_t *lock);
/* 极短寄存器读改写临界区，可嵌套，不在临界区中等待硬件。 */
long bsp_irq_save(void);
void bsp_irq_restore(long level);
#endif
