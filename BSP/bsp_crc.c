#include "bsp_crc.h"
#include "bsp_device_internal.h"
static bsp_device_t crc;

bsp_status_t bsp_crc_init(void)
{
    bsp_status_t result = bsp_device_enter(&crc, 0);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    if (RCC->AHBPCENR & RCC_AHBPeriph_CRC) result = BSP_ERR_BUSY;
    else RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, ENABLE);
    bsp_irq_restore(level);
    bsp_device_leave(&crc, result == BSP_OK);
    return result;
}

bsp_status_t bsp_crc_words(const uint32_t *words, size_t count, uint32_t *value)
{
    if (!value || (count && !words)) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&crc, 1);
    if (result != BSP_OK) return result;
    CRC_ResetDR();
    for (size_t i = 0; i < count; ++i) CRC_CalcCRC(words[i]);
    *value = CRC_GetCRC();
    bsp_device_leave(&crc, 1);
    return BSP_OK;
}

bsp_status_t bsp_crc_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&crc, 1);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    CRC_ResetDR();
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CRC, DISABLE);
    bsp_irq_restore(level);
    bsp_device_leave(&crc, 0);
    return BSP_OK;
}
