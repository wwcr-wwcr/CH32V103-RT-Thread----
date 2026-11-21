#ifndef BSP_RTC_H
#define BSP_RTC_H
#include "bsp_types.h"
/* 使用板上 32.768 kHz LSE，建立 1 Hz 秒计数。保留已有计数值；重设预分频可改变亚秒相位。 */
bsp_status_t bsp_rtc_init(void);
/* 读取连续的 32 位原始秒计数，跨低 16 位进位时自动重读。 */
bsp_status_t bsp_rtc_read(uint32_t *seconds);
/* 写入秒计数。调用者决定纪元，不进行日期或时区换算。 */
bsp_status_t bsp_rtc_write(uint32_t seconds);
/* 释放 CPU 侧接口。RTC 与 LSE 保持计时，以便下次初始化接续。 */
bsp_status_t bsp_rtc_de_init(void);
#endif
