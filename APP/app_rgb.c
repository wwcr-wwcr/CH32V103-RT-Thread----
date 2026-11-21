#include "app_rgb.h"
#include "app_internal.h"
#include "private/app_output_pattern.h"

static const bsp_pin_t pins[] = {BSP_PIN_RGB_RED, BSP_PIN_RGB_GREEN, BSP_PIN_RGB_BLUE};
#define COLOR_COUNT (sizeof pins / sizeof pins[0])
static app_device_t device;
static app_output_pattern_t blink;
static app_status_t write_color(uint16_t color);

app_status_t app_rgb_init(void)
{
    app_status_t result = app_device_enter(&device, 0);
    if (result != APP_OK) return result;
    size_t acquired = 0;
    while (acquired < COLOR_COUNT) {
        result = app_from_bsp(bsp_gpio_init(pins[acquired], GPIO_Mode_Out_PP, 1));
        if (result != APP_OK) break;
        acquired++;
    }
    if (result != APP_OK)
        while (acquired) bsp_gpio_de_init(pins[--acquired]);
    if (result == APP_OK) app_output_pattern_init(&blink, "rgb_seq", write_color, NULL);
    app_device_leave(&device, result == APP_OK);
    return result;
}

static app_status_t write_color(uint16_t color)
{
    app_status_t result = APP_OK;
    rt_base_t level = rt_hw_interrupt_disable();
    for (size_t i = 0; i < COLOR_COUNT; ++i) {
        result = app_from_bsp(bsp_gpio_write(pins[i], !(color & (1U << i))));
        if (result != APP_OK) break;
    }
    rt_hw_interrupt_enable(level);
    return result;
}

app_status_t app_rgb_set(app_rgb_color_t color)
{
    if ((unsigned)color > APP_RGB_WHITE) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    result = app_output_pattern_set(&blink, color);
    app_device_leave(&device, 1);
    return result;
}

static app_status_t start_blink(app_rgb_color_t first, uint32_t first_ms, app_rgb_color_t second,
                                uint32_t second_ms, uint32_t count)
{
    app_output_phase_t a = {.level = first}, b = {.level = second};
    if ((unsigned)first > APP_RGB_WHITE || first == APP_RGB_OFF || !count ||
        !app_output_durations(first_ms, second_ms, &a, &b)) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    if (blink.remaining) result = APP_ERR_BUSY;
    else result = app_output_pattern_start(&blink, a, b, count);
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_rgb_blink(app_rgb_color_t color, uint32_t on_ms, uint32_t off_ms, uint32_t count)
{
    return start_blink(color, on_ms, APP_RGB_OFF, off_ms, count);
}

app_status_t app_rgb_blink_alternate(app_rgb_color_t first, uint32_t first_ms,
                                     app_rgb_color_t second, uint32_t second_ms, uint32_t count)
{
    if ((unsigned)second > APP_RGB_WHITE || second == APP_RGB_OFF || first == second || !second_ms)
        return APP_ERR_ARG;
    return start_blink(first, first_ms, second, second_ms, count);
}

app_status_t app_rgb_de_init(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    /* 写入失败时保留 GPIO 和重试定时器，调用者可稍后再次解初始化。 */
    result = app_output_pattern_set(&blink, APP_RGB_OFF);
    if (result != APP_OK) {
        app_device_leave(&device, 1);
        return result;
    }
    app_output_pattern_release(&blink);
    for (size_t i = 0; i < COLOR_COUNT; ++i) {
        app_status_t release = app_from_bsp(bsp_gpio_de_init(pins[i]));
        if (result == APP_OK) result = release;
    }
    app_device_leave(&device, 0);
    return result;
}
