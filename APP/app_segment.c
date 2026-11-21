#include "app_segment.h"
#include "app_internal.h"

#define SYS_COMMAND 0x48U
#define DIGIT_COMMAND 0x70U
#define ENABLE_SCAN 0x05U
#define POWER_ON_RESET_MS 80
#define SEG_POINT 0x20U
#define SEG_MINUS 0x08U
/* ESAD2025 段线排列，与通用的 abcdefg 位序不同。 */
static const uint8_t codes[10] =
    {0xd7, 0x14, 0xcd, 0x5d, 0x1e, 0x5b, 0xdb, 0x15, 0xdf, 0x5f};
static app_device_t device;

/* 板级 BSP GPIO 调用之间另留建立/保持时间，不占用系统节拍定时器。 */
static void drive(bsp_pin_t pin, uint8_t high, app_status_t *result)
{
    app_status_t status = app_from_bsp(bsp_gpio_write(pin, high));
    if (*result == APP_OK) *result = status;
    for (volatile unsigned i = 0; i < 64; ++i) { }
}

static void write_byte(uint8_t byte, app_status_t *result)
{
    /* CH422 的第九个时钟期间 SDA 固定为 1。 */
    uint16_t bits = ((uint16_t)byte << 1) | 1U;
    for (uint16_t mask = 0x100; mask; mask >>= 1) {
        drive(BSP_PIN_LED_SDA, !!(mask & bits), result);
        drive(BSP_PIN_LED_SCL, 1, result);
        drive(BSP_PIN_LED_SCL, 0, result);
    }
}

static app_status_t write_command(uint8_t cmd, uint8_t value)
{
    app_status_t result = APP_OK;
    drive(BSP_PIN_LED_SDA, 0, &result); /* SCL 空闲为高，SDA 下降形成 START。 */
    drive(BSP_PIN_LED_SCL, 0, &result);
    write_byte(cmd, &result);
    write_byte(value, &result);
    drive(BSP_PIN_LED_SDA, 0, &result);
    drive(BSP_PIN_LED_SCL, 1, &result);
    drive(BSP_PIN_LED_SDA, 1, &result);
    return result;
}

static app_status_t write_digits(const uint8_t digits[4])
{
    for (unsigned i = 0; i < 4; ++i) {
        app_status_t result = write_command(DIGIT_COMMAND + 2U * i, digits[i]);
        if (result != APP_OK) return result;
    }
    return APP_OK;
}

app_status_t app_segment_init(void)
{
    app_status_t result = app_device_enter(&device, 0);
    if (result != APP_OK) return result;
    /* CH422 上电复位最长 80 ms，期间可能忽略写命令。 */
    rt_tick_t ready_tick = rt_tick_from_millisecond(POWER_ON_RESET_MS);
    rt_tick_t now = rt_tick_get();
    if (now < ready_tick) {
        if (app_can_sleep()) {
            uint32_t ms = ((ready_tick - now) * 1000U + RT_TICK_PER_SECOND - 1U) / RT_TICK_PER_SECOND;
            rt_thread_mdelay((rt_int32_t)ms);
        }
        if (rt_tick_get() < ready_tick) {
            app_device_leave(&device, 0);
            return APP_ERR_BUSY;
        }
    }
    result = app_from_bsp(bsp_gpio_init(BSP_PIN_LED_SCL, GPIO_Mode_Out_PP, 1));
    if (result == APP_OK) {
        result = app_from_bsp(bsp_gpio_init(BSP_PIN_LED_SDA, GPIO_Mode_Out_OD, 1));
        if (result == APP_OK) {
            /* 先清段存储，再打开扫描，避免上次会话内容短暂出现。 */
            result = write_digits((uint8_t[4]){0});
            if (result == APP_OK) result = write_command(SYS_COMMAND, ENABLE_SCAN);
            if (result != APP_OK) bsp_gpio_de_init(BSP_PIN_LED_SDA);
        }
        if (result != APP_OK) bsp_gpio_de_init(BSP_PIN_LED_SCL);
    }
    app_device_leave(&device, result == APP_OK);
    return result;
}

app_status_t app_segment_de_init(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = write_command(SYS_COMMAND, 0);
    app_status_t sda = app_from_bsp(bsp_gpio_de_init(BSP_PIN_LED_SDA));
    app_status_t scl = app_from_bsp(bsp_gpio_de_init(BSP_PIN_LED_SCL));
    app_device_leave(&device, 0);
    if (result == APP_OK) result = sda == APP_OK ? scl : sda;
    return result;
}

static app_status_t display(const uint8_t digits[4])
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = write_digits(digits);
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_segment_show_fixed(int32_t value, uint8_t decimals)
{
    if (value < -999 || value > 9999 || decimals > 3 || (value < 0 && decimals == 3))
        return APP_ERR_ARG;
    unsigned magnitude = (unsigned)(value < 0 ? -value : value);
    uint8_t digits[4] = {0};
    int pos = 3;
    do {
        digits[pos--] = codes[magnitude % 10U];
        magnitude /= 10U;
    } while (magnitude || pos >= 3 - (int)decimals);
    if (value < 0) digits[pos] = SEG_MINUS;
    if (decimals) digits[3U - decimals] |= SEG_POINT;
    return display(digits);
}

app_status_t app_segment_show_number(int32_t value)
{
    return app_segment_show_fixed(value, 0);
}

app_status_t app_segment_show_text(const char *text)
{
    if (!text) return APP_ERR_ARG;
    uint8_t digits[4] = {0};
    unsigned used = 0;
    for (; *text; ++text) {
        char c = *text;
        if (c == '.' && used && !(digits[used - 1] & SEG_POINT)) {
            digits[used - 1] |= SEG_POINT;
            continue;
        }
        if (used == sizeof digits) return APP_ERR_ARG;
        if (c >= '0' && c <= '9') digits[used] = codes[c - '0'];
        else if (c == '-') digits[used] = SEG_MINUS;
        else if (c == '.') digits[used] = SEG_POINT;
        else if (c != ' ') return APP_ERR_ARG;
        ++used;
    }
    return display(digits);
}

app_status_t app_segment_clear(void)
{
    return display((uint8_t[4]){0});
}
