#include "app_rtc.h"
#include "app_internal.h"

static unsigned leap(unsigned year)
{
    return year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
}
static unsigned month_days(unsigned year, unsigned month)
{
    static const uint8_t days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return days[month - 1] + (month == 2 && leap(year));
}
app_status_t app_rtc_init(void)
{
    return app_from_bsp(bsp_rtc_init());
}
app_status_t app_rtc_de_init(void)
{
    return app_from_bsp(bsp_rtc_de_init());
}
app_status_t app_rtc_set_time(const app_rtc_time_t *time)
{
    if (!time || time->year < 1970 || time->year > 2106 ||
        time->month < 1 || time->month > 12 || time->day < 1 ||
        time->day > month_days(time->year, time->month) ||
        time->hour > 23 || time->minute > 59 || time->second > 59) return APP_ERR_ARG;
    uint32_t days = time->day - 1U;
    for (unsigned y = 1970; y < time->year; ++y) days += 365 + leap(y);
    for (unsigned m = 1; m < time->month; ++m) days += month_days(time->year, m);
    uint64_t seconds = days * UINT64_C(86400) + time->hour * 3600U + time->minute * 60U + time->second;
    if (seconds > UINT32_MAX) return APP_ERR_ARG;
    return app_from_bsp(bsp_rtc_write((uint32_t)seconds));
}
app_status_t app_rtc_get_time(app_rtc_time_t *time)
{
    if (!time) return APP_ERR_ARG;
    uint32_t seconds;
    app_status_t rc = app_from_bsp(bsp_rtc_read(&seconds));
    if (rc != APP_OK) return rc;
    uint32_t days = seconds / 86400U;
    unsigned y = 1970, m = 1;
    while (days >= 365 + leap(y)) { days -= 365 + leap(y); ++y; }
    while (days >= month_days(y, m)) { days -= month_days(y, m); ++m; }
    *time = (app_rtc_time_t){y, m, days + 1, (seconds / 3600U) % 24U,
                             (seconds / 60U) % 60U, seconds % 60U};
    return APP_OK;
}
