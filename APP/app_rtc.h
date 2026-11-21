#ifndef APP_RTC_H
#define APP_RTC_H

#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t year;
    uint8_t month, day, hour, minute, second;
} app_rtc_time_t;
/* 启动板载 32.768 kHz 晶振并接管秒计数；保留已有时间。 */
app_status_t app_rtc_init(void);
/* 释放 CPU 接口，备份域继续计时。 */
app_status_t app_rtc_de_init(void);
/* 读写年月日时分秒。支持 1970-01-01 至 2106-02-07 06:28:15，
 * 采用同一时区读写，无夏令时与闰秒换算。未设置且计数为零时读到起始日期。 */
app_status_t app_rtc_get_time(app_rtc_time_t *time);
app_status_t app_rtc_set_time(const app_rtc_time_t *time);

#ifdef __cplusplus
}
#endif
#endif
