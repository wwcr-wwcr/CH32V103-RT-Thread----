#include "bsp_bkp.h"
#include "bsp_device_internal.h"

static bsp_device_t bkp;

static uint16_t reg_offset(uint8_t i)
{
    return i <= 10 ? BKP_DR1 + 4U * (i - 1U) : BKP_DR11 + 4U * (i - 11U);
}

bsp_status_t bsp_bkp_init(void)
{
    bsp_status_t result = bsp_device_enter(&bkp, 0);
    if (result == BSP_OK) {
        bsp_power_clock_get(1);
        bsp_device_leave(&bkp, 1);
    }
    return result;
}

bsp_status_t bsp_bkp_read(uint8_t index, uint16_t *value)
{
    if (!value || index < 1 || index > 42) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&bkp, 1);
    if (result == BSP_OK) {
        *value = BKP_ReadBackupRegister(reg_offset(index));
        bsp_device_leave(&bkp, 1);
    }
    return result;
}

bsp_status_t bsp_bkp_write(uint8_t index, uint16_t value)
{
    if (index < 1 || index > 42) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&bkp, 1);
    if (result != BSP_OK) return result;
    result = bsp_backup_write_begin();
    if (result == BSP_OK) {
        BKP_WriteBackupRegister(reg_offset(index), value);
        bsp_backup_write_end();
    }
    bsp_device_leave(&bkp, 1);
    return result;
}

bsp_status_t bsp_bkp_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&bkp, 1);
    if (result == BSP_OK) {
        bsp_power_clock_put(1);
        bsp_device_leave(&bkp, 0);
    }
    return result;
}
