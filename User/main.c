/* CH32V307VCT6：LVGL 通过 INIT_APP_EXPORT 自动启动。 */
#include "ch32v30x.h"
#include <rtthread.h>
int main(void)
{
    SystemCoreClockUpdate();
    rt_kprintf("CH32V307VCT6 RT-Thread + LVGL 8.3.11\n");
    return 0;
}
