#include "ch422.h"
#include "gpio.h"
#include <rtthread.h>
#include <rthw.h>
#include "lvgl/src/misc/lv_printf.h"
#include <stdarg.h>
#include <string.h>

#define SCL GPIO_Pin_0
#define SDA GPIO_Pin_1
static int ready;
static struct rt_mutex mutex;
static int ch422_ipc_init(void) { return rt_mutex_init(&mutex, "ch422", RT_IPC_FLAG_PRIO); }
INIT_COMPONENT_EXPORT(ch422_ipc_init);

static void drive(uint16_t pin, int high)
{
    GPIO_WriteBit(GPIOE, pin, high ? Bit_SET : Bit_RESET);
    rt_hw_us_delay(2);
}

static void write_byte(uint8_t byte)
{
    /* 第九个时钟释放 SDA。 */
    uint16_t bits = ((uint16_t)byte << 1) | 1U;
    for (uint16_t mask = 0x100; mask; mask >>= 1) {
        drive(SDA, bits & mask);
        drive(SCL, 1);
        drive(SCL, 0);
    }
}

static void write_frame(uint8_t command, uint8_t value)
{
    drive(SDA, 0);
    drive(SCL, 0);
    write_byte(command);
    write_byte(value);
    drive(SDA, 0);
    drive(SCL, 1);
    drive(SDA, 1);
}

static int segment_code(char c)
{
    static const char symbols[] = "0123456789- ABCDEFHLPU";
    static const uint8_t codes[] = {
        0xd7,0x14,0xcd,0x5d,0x1e,0x5b,0xdb,0x15,0xdf,0x5f,
        0x08,0x00,0x9f,0xda,0xc3,0xdc,0xcb,0x8b,0x9e,0xc2,0x8f,0xd6
    };
    if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
    const char *pos = strchr(symbols, c);
    return pos ? codes[pos - symbols] : -1;
}

int CH422_Show(const char *format, ...)
{
    char text[24];
    uint8_t digits[4] = {0};
    if (!format) return -RT_EINVAL;
    va_list args;
    va_start(args, format);
    int len = lv_vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (len < 0) return -RT_EINVAL;

    unsigned used = 0;
    for (const char *p = text; *p; ++p) {
        if (*p == '.') {
            if (used) digits[used - 1] |= 0x20;
        } else {
            if (used == 4) break;
            int code = segment_code(*p);
            digits[used++] = code < 0 ? 0 : (uint8_t)code;
        }
    }

    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc < 0) return rc;
    if (!ready) {
        gpio_config(GPIOE, SCL, GPIO_Mode_IN_FLOATING);
        GPIO_SetBits(GPIOE, SCL | SDA);
        gpio_config(GPIOE, SCL, GPIO_Mode_Out_PP);
        gpio_config(GPIOE, SDA, GPIO_Mode_Out_OD);
        rt_thread_mdelay(81); /* CH422 上电稳定时间，仅首次调用。 */
    }
    for (unsigned i = 0; i < 4; ++i) write_frame(0x70U + 2U * i, digits[i]);
    if (!ready) write_frame(0x48, 0x05);
    ready = 1;
    rt_mutex_release(&mutex);
    return RT_EOK;
}
