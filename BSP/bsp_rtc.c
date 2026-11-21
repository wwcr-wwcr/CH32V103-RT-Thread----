#include "bsp_rtc.h"
#include "bsp_device_internal.h"

#define BSP_RTC_SYNC_POLLS 1000000UL
#define BSP_LSE_START_POLLS 16000000UL
static bsp_device_t rtc;

static bsp_status_t wait_flag(uint16_t flag)
{
    for (uint32_t i = 0; i < BSP_RTC_SYNC_POLLS; ++i)
        if (RTC_GetFlagStatus(flag) == SET) return BSP_OK;
    return BSP_ERR_TIMEOUT;
}

static bsp_status_t configure(void)
{
    uint32_t previous = RCC->BDCTLR;
    uint32_t source = previous & RCC_RTCSEL;
    if (source && source != RCC_RTCCLKSource_LSE) return BSP_ERR_BUSY;
    if (!(previous & RCC_LSEON)) RCC_LSEConfig(RCC_LSE_ON);
    uint32_t n = BSP_LSE_START_POLLS;
    while (n && RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET) --n;
    if (!n) {
        if (!(previous & RCC_LSEON)) RCC_LSEConfig(RCC_LSE_OFF);
        return BSP_ERR_TIMEOUT;
    }
    if (!source) RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
    RCC_RTCCLKCmd(ENABLE);
    RTC_ClearFlag(RTC_FLAG_RSF);
    bsp_status_t result = wait_flag(RTC_FLAG_RSF);
    /* PSCRH/L 为只写。接管已有 RTC 时也应建立确定的 1 Hz 单位。 */
    if (result == BSP_OK) {
        result = wait_flag(RTC_FLAG_RTOFF);
        if (result == BSP_OK) {
            RTC_SetPrescaler(32767);
            result = wait_flag(RTC_FLAG_RTOFF);
        }
    }
    if (result != BSP_OK && !(previous & RCC_RTCEN)) {
        RCC_RTCCLKCmd(DISABLE);
        if (!(previous & RCC_LSEON)) RCC_LSEConfig(RCC_LSE_OFF);
    }
    return result;
}

bsp_status_t bsp_rtc_init(void)
{
    bsp_status_t result = bsp_device_enter(&rtc, 0);
    if (result != BSP_OK) return result;
    bsp_power_clock_get(1);
    result = bsp_backup_write_begin();
    if (result == BSP_OK) {
        result = configure();
        bsp_backup_write_end();
    }
    if (result != BSP_OK) bsp_power_clock_put(1);
    bsp_device_leave(&rtc, result == BSP_OK);
    return result;
}

bsp_status_t bsp_rtc_read(uint32_t *seconds)
{
    if (!seconds) return BSP_ERR_ARG;
    bsp_status_t result = bsp_device_enter(&rtc, 1);
    if (result != BSP_OK) return result;
    result = BSP_ERR_TIMEOUT;
    for (unsigned i = 0; i < 8; ++i) {
        uint32_t high = RTC->CNTH;
        if (high != RTC->CNTH) continue;
        uint16_t low = RTC->CNTL;
        if (low == RTC->CNTL && high == RTC->CNTH) {
            *seconds = (high << 16) | low;
            result = BSP_OK;
            break;
        }
    }
    bsp_device_leave(&rtc, 1);
    return result;
}

bsp_status_t bsp_rtc_write(uint32_t seconds)
{
    bsp_status_t result = bsp_device_enter(&rtc, 1);
    if (result != BSP_OK) return result;
    result = bsp_backup_write_begin();
    if (result == BSP_OK) {
        result = wait_flag(RTC_FLAG_RTOFF);
        if (result == BSP_OK) {
            RTC_SetCounter(seconds);
            result = wait_flag(RTC_FLAG_RTOFF);
        }
        bsp_backup_write_end();
    }
    bsp_device_leave(&rtc, 1);
    return result;
}

bsp_status_t bsp_rtc_de_init(void)
{
    bsp_status_t result = bsp_device_enter(&rtc, 1);
    if (result == BSP_OK) {
        bsp_power_clock_put(1);
        bsp_device_leave(&rtc, 0);
    }
    return result;
}
