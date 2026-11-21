#include "bsp_flash.h"
#include "bsp_device_internal.h"
#include <string.h>

extern const char __bsp_image_end[], __bsp_rom_end[];
static bsp_device_t flash;
static uint32_t base;
static size_t length;

/* RM 32.6，VCT6 的用户选项字 USER[7:5] 决定 Flash/SRAM 分配。 */
static size_t flash_capacity(void)
{
    static const uint16_t kib[] = {192, 192, 224, 224, 256, 256, 128, 288};
    uint16_t user_pair = OB->USER;
    uint8_t user = (uint8_t)user_pair;
    uint32_t loaded = FLASH->OBR;
    if ((loaded & 1U) || (uint8_t)(user ^ (user_pair >> 8)) != 0xffU ||
        ((loaded >> 8) & 3U) != (user >> 6)) return 0;
    return kib[user >> 5] * 1024U;
}

bsp_status_t bsp_flash_init(uint32_t start, size_t bytes)
{
    size_t capacity = flash_capacity();
    if (!capacity) return BSP_ERR_STATE;
    if (capacity > (uintptr_t)__bsp_rom_end) capacity = (uintptr_t)__bsp_rom_end;
    uint32_t end = FLASH_BASE + capacity;
    if (start < FLASH_BASE + (uintptr_t)__bsp_image_end || start >= end ||
        !bytes || bytes > end - start || ((start | bytes) % BSP_FLASH_PAGE_BYTES) != 0)
        return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&flash, 0);
    if (result == BSP_OK) {
        base = start;
        length = bytes;
        bsp_device_leave(&flash, 1);
    }
    return result;
}

bsp_status_t bsp_flash_read(size_t offset, void *dst, size_t count)
{
    if (!dst || !count) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&flash, 1);
    if (result != BSP_OK) return result;
    if (offset >= length || count > length - offset) result = BSP_ERR_ARG;
    else memcpy(dst, (const void *)(base + offset), count);
    bsp_device_leave(&flash, 1);
    return result;
}

static bsp_status_t status(FLASH_Status s)
{
    if (s == FLASH_COMPLETE) return BSP_OK;
    return s == FLASH_TIMEOUT ? BSP_ERR_TIMEOUT : BSP_ERR_IO;
}

static bsp_status_t unlock_flash(void)
{
    if (FLASH->STATR & FLASH_STATR_EHMODS) return BSP_ERR_STATE;
    if ((FLASH->STATR & (FLASH_FLAG_BSY | FLASH_STATR_WRBSY)) || !(FLASH->CTLR & FLASH_CTLR_LOCK))
        return BSP_ERR_BUSY;
    FLASH_Unlock();
    if (FLASH->CTLR & FLASH_CTLR_LOCK) return BSP_ERR_IO;
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_WRPRTERR);
    return BSP_OK;
}

bsp_status_t bsp_flash_erase_page(size_t offset)
{
    bsp_status_t result = bsp_device_enter(&flash, 1);
    if (result != BSP_OK) return result;
    if (offset >= length || offset % BSP_FLASH_PAGE_BYTES) result = BSP_ERR_ARG;
    else if ((result = unlock_flash()) == BSP_OK) {
        result = status(FLASH_ErasePage(base + offset));
        FLASH_Lock();
        if (result == BSP_OK) {
            const volatile uint16_t *page = (const volatile uint16_t *)(base + offset);
            for (size_t i = 0; i < BSP_FLASH_PAGE_BYTES / sizeof(*page); ++i)
                if (page[i] != BSP_FLASH_ERASED_HALFWORD) {
                    result = BSP_ERR_IO;
                    break;
                }
        }
    }
    bsp_device_leave(&flash, 1);
    return result;
}

bsp_status_t bsp_flash_write(size_t offset, const uint16_t *src, size_t count)
{
    if (!src || !count || (offset & 1) || ((uintptr_t)src & 1)) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&flash, 1);
    if (result != BSP_OK) return result;
    if (offset >= length || count > (length - offset) / 2) result = BSP_ERR_ARG;
    if (result == BSP_OK) {
        volatile uint16_t *target = (volatile uint16_t *)(base + offset);
        for (size_t i = 0; i < count; ++i)
            if (target[i] != BSP_FLASH_ERASED_HALFWORD && target[i] != src[i]) { result = BSP_ERR_STATE; break; }
        if (result == BSP_OK && (result = unlock_flash()) == BSP_OK) {
            for (size_t i = 0; i < count && result == BSP_OK; ++i) {
                if (target[i] == src[i]) continue;
                result = status(FLASH_ProgramHalfWord((uintptr_t)&target[i], src[i]));
                if (result == BSP_OK && target[i] != src[i]) result = BSP_ERR_IO;
            }
            FLASH_Lock();
        }
    }
    bsp_device_leave(&flash, 1);
    return result;
}

bsp_status_t bsp_flash_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&flash, 1);
    if (result == BSP_OK) {
        base = 0;
        length = 0;
        bsp_device_leave(&flash, 0);
    }
    return result;
}
