#ifndef BSP_FLASH_H
#define BSP_FLASH_H
#include "bsp_types.h"

#define BSP_FLASH_PAGE_BYTES 4096U
#define BSP_FLASH_ERASED_HALFWORD 0xE339U  /* WCH FLASH_Program 例程中的擦除读值 */
/* 打开一段用户数据区。start 是 0x08000000 地址空间中的物理地址；
 * 起点和 bytes 均须按 4 KiB 整页对齐，范围须在本固件镜像末尾之后，受链接区域和实际选项字容量的共同限制。 */
bsp_status_t bsp_flash_init(uint32_t start, size_t bytes);
/* 从所申请区域的字节偏移读取 count 字节到 RAM。 */
bsp_status_t bsp_flash_read(size_t offset, void *dst, size_t count);
/* 擦除 offset 处的一整页。offset 必须 4 KiB 对齐。 */
bsp_status_t bsp_flash_erase_page(size_t offset);
/* 从 RAM 半字数组写入 count 个半字。offset 为偶数字节偏移；
 * 目的值必须是擦除态或已等于对应值。硬件错误时先前写入的半字可能已生效。 */
bsp_status_t bsp_flash_write(size_t offset, const uint16_t *src, size_t count);
/* 归还区域，保留已保存数据。 */
bsp_status_t bsp_flash_de_init(void);
#endif
