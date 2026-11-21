#include "bsp_pwr.h"
#include "bsp_device_internal.h"

static bsp_device_t pvd;
static uint8_t pwr_users, bkp_users, pwr_was_on, bkp_was_on;
static uint8_t backup_lock, was_writable;
static uint32_t saved_level;

static void clock_get(uint32_t clock, uint8_t *users, uint8_t *was_on)
{
    if ((*users)++ == 0) {
        *was_on = (RCC->APB1PCENR & clock) != 0;
        RCC_APB1PeriphClockCmd(clock, ENABLE);
    }
}

static void clock_put(uint32_t clock, uint8_t *users, uint8_t was_on)
{
    if (--(*users) == 0 && !was_on) RCC_APB1PeriphClockCmd(clock, DISABLE);
}

void bsp_power_clock_get(int backup)
{
    long level = bsp_irq_save();
    clock_get(RCC_APB1Periph_PWR, &pwr_users, &pwr_was_on);
    if (backup) clock_get(RCC_APB1Periph_BKP, &bkp_users, &bkp_was_on);
    bsp_irq_restore(level);
}

void bsp_power_clock_put(int backup)
{
    long level = bsp_irq_save();
    if (backup) clock_put(RCC_APB1Periph_BKP, &bkp_users, bkp_was_on);
    clock_put(RCC_APB1Periph_PWR, &pwr_users, pwr_was_on);
    bsp_irq_restore(level);
}

bsp_status_t bsp_backup_write_begin(void)
{
    bsp_status_t result = bsp_try_lock(&backup_lock);
    if (result == BSP_OK) {
        long level = bsp_irq_save();
        was_writable = (PWR->CTLR & PWR_CTLR_DBP) != 0;
        PWR_BackupAccessCmd(ENABLE);
        bsp_irq_restore(level);
    }
    return result;
}

void bsp_backup_write_end(void)
{
    long level = bsp_irq_save();
    if (!was_writable) PWR_BackupAccessCmd(DISABLE);
    bsp_unlock(&backup_lock);
    bsp_irq_restore(level);
}

bsp_status_t bsp_pwr_init(uint32_t level)
{
    if (level & ~(uint32_t)PWR_CTLR_PLS) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&pvd, 0);
    if (result != BSP_OK) return result;
    bsp_power_clock_get(0);
    long key = bsp_irq_save();
    if (PWR->CTLR & PWR_CTLR_PVDE) result = BSP_ERR_BUSY;
    else {
        saved_level = PWR->CTLR & PWR_CTLR_PLS;
        PWR_PVDLevelConfig(level);
        PWR_PVDCmd(ENABLE);
    }
    bsp_irq_restore(key);
    if (result != BSP_OK) bsp_power_clock_put(0);
    bsp_device_leave(&pvd, result == BSP_OK);
    return result;
}

bsp_status_t bsp_pwr_read(uint8_t *below)
{
    if (!below) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&pvd, 1);
    if (result == BSP_OK) {
        *below = (PWR->CSR & PWR_CSR_PVDO) != 0;
        bsp_device_leave(&pvd, 1);
    }
    return result;
}

bsp_status_t bsp_pwr_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&pvd, 1);
    if (result != BSP_OK) return result;
    long level = bsp_irq_save();
    PWR_PVDCmd(DISABLE);
    PWR_PVDLevelConfig(saved_level);
    bsp_irq_restore(level);
    bsp_power_clock_put(0);
    bsp_device_leave(&pvd, 0);
    return BSP_OK;
}
