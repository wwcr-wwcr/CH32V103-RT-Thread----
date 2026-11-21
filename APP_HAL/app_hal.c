#include "app_hal.h"
#include "app_hal_internal.h"
#include "app.h"
#include <rtthread.h>
#include <rthw.h>
#include <string.h>

#define MIC_BITS (HAL_SENSOR_BIT(HAL_DBFS) | HAL_SENSOR_BIT(HAL_SPL) | HAL_SENSOR_BIT(HAL_HZ))
#define VALID_BITS ((1UL << 14) - 1)
#define TEXT_SIZE 9 /* 四位，每位可附一个小数点，再加字符串结束符。 */

typedef struct {
    unsigned value; /* LED 颜色或蜂鸣器频率；0 表示关闭。 */
    uint32_t on, gap;
} signal_t;

typedef struct {
    char text[TEXT_SIZE];
    unsigned priority;
    uint8_t has_text;
    signal_t light, sound;
} alert_t;

typedef struct {
    uint32_t mask;
    app_status_t (*update)(void);
    uint8_t open;
    char page[TEXT_SIZE];
    app_rgb_color_t color;
    alert_t alert;
} client_t;

typedef struct {
    uint32_t bits;
    app_status_t (*init)(void);
    app_status_t (*release)(void);
} device_t;
static const device_t devices[] = {
    {HAL_SEG, app_segment_init, app_segment_de_init}, {HAL_RGB, app_rgb_init, app_rgb_de_init},
    {HAL_BUZZ, app_buzzer_init, app_buzzer_de_init}, {HAL_KEYS, app_key_init, app_key_de_init},
    {HAL_RTC, app_rtc_init, app_rtc_de_init}, {HAL_FAN_DEV, app_fan_init, app_fan_de_init},
    {HAL_ENCODER, app_encoder_init, app_encoder_de_init}, {HAL_IMU, app_imu_init, app_imu_de_init},
    {HAL_SENSOR_BIT(HAL_TEMP), app_ntc_init, app_ntc_de_init},
    {HAL_SENSOR_BIT(HAL_LUX), app_light_init, app_light_de_init},
    {MIC_BITS, app_mic_init, app_mic_de_init},
    {HAL_SENSOR_BIT(HAL_DISTANCE), app_ultrasonic_init, app_ultrasonic_de_init}
};

static client_t slots[HAL_OWNER_COUNT];
static hal_owner_t pages[HAL_OWNER_COUNT];
static unsigned page_count;
static rt_thread_t service_thread;
static uint8_t in_update;
static uint8_t key_events[4], key_down[4];
static hal_owner_t key_target = HAL_OWNER_COUNT;
static uint32_t applied;
static char shown[TEXT_SIZE];
static signal_t lit, sounding;
static struct { app_status_t result; float value; uint8_t valid; } samples[HAL_SENSOR_COUNT];
static app_status_t route_keys(int collect);
enum { SLOT_ANY, SLOT_ACTIVE, SLOT_CLOSING };

static int on_thread(void)
{
    return service_thread && !rt_interrupt_get_nest() && rt_thread_self() == service_thread;
}

uint32_t hal_now_ms(void)
{
#if RT_TICK_PER_SECOND == 1000
    return rt_tick_get();
#else
    static rt_tick_t previous;
    static uint32_t ms, fraction;
    rt_tick_t now = rt_tick_get();
    uint64_t units = (uint64_t)(rt_tick_t)(now - previous) * 1000U + fraction;
    previous = now;
    ms += (uint32_t)(units / RT_TICK_PER_SECOND);
    fraction = (uint32_t)(units % RT_TICK_PER_SECOND);
    return ms;
#endif
}

app_status_t app_hal_init(void)
{
    if (rt_interrupt_get_nest() || !rt_thread_self()) return APP_ERR_STATE;
    rt_base_t irq = rt_hw_interrupt_disable();
    app_status_t rc = service_thread && service_thread != rt_thread_self() ? APP_ERR_BUSY : APP_OK;
    if (rc == APP_OK) service_thread = rt_thread_self();
    rt_hw_interrupt_enable(irq);
    return rc;
}

int hal_active(hal_owner_t owner)
{
    return (unsigned)owner < HAL_OWNER_COUNT && on_thread() && slots[owner].open == SLOT_ACTIVE;
}

/* 释放失败的设备仍被占用，但在重试释放前停止读写。state 为 0 时统计所有占用。 */
static uint32_t device_mask(hal_owner_t except, unsigned state)
{
    uint32_t bits = 0;
    for (unsigned i = 0; i < HAL_OWNER_COUNT; ++i)
        if (i != (unsigned)except && (state == SLOT_ANY || slots[i].open == state))
            bits |= slots[i].mask;
    return bits;
}

static void first_error(app_status_t *result, app_status_t status)
{
    if (status != APP_OK && (*result == APP_OK || *result == APP_ERR_BUSY))
        *result = status;
}

static app_status_t release_mask(hal_owner_t owner, uint32_t mask)
{
    client_t *s = &slots[owner];
    uint32_t other = device_mask(owner, SLOT_ANY);
    app_status_t result = APP_OK;
    for (size_t i = sizeof devices / sizeof devices[0]; i-- > 0;) {
        const device_t *d = &devices[i];
        uint32_t released = s->mask & mask & d->bits;
        if (!released) continue;
        if (!((other | (s->mask & ~released)) & d->bits)) {
            app_status_t rc = d->release();
            if (rc != APP_OK) { first_error(&result, rc); continue; }
            applied &= ~d->bits;
            for (unsigned k = 0; k < HAL_SENSOR_COUNT; ++k)
                if (d->bits & HAL_SENSOR_BIT(k)) samples[k].valid = 0;
        }
        s->mask &= ~released;
    }
    return result;
}

app_status_t hal_drop(hal_owner_t owner, uint32_t mask)
{
    return hal_active(owner) ? release_mask(owner, mask) : APP_ERR_STATE;
}

app_status_t hal_require(hal_owner_t owner, uint32_t mask)
{
    if (!hal_active(owner)) return APP_ERR_STATE;
    if (mask & ~VALID_BITS) return APP_ERR_ARG;
    client_t *s = &slots[owner];
    uint32_t original = s->mask, held = device_mask(HAL_OWNER_COUNT, SLOT_ANY);
    uint32_t closing = device_mask(HAL_OWNER_COUNT, SLOT_CLOSING);
    /* 如果最后一次释放失败，该设备可能已部分关闭，不能交给新用户。 */
    for (size_t i = 0; i < sizeof devices / sizeof devices[0]; ++i)
        if ((mask & devices[i].bits) && (closing & devices[i].bits)) return APP_ERR_BUSY;
    for (size_t i = 0; i < sizeof devices / sizeof devices[0]; ++i) {
        const device_t *d = &devices[i];
        if (!(mask & d->bits)) continue;
        if (!(held & d->bits)) {
            app_status_t rc = d->init();
            if (rc != APP_OK) {
                app_status_t rollback = hal_drop(owner, s->mask & ~original);
                return rollback == APP_OK ? rc : rollback;
            }
            applied &= ~d->bits;
        }
        s->mask |= mask & d->bits;
    }
    return APP_OK;
}

app_status_t hal_open(hal_owner_t owner, uint32_t mask)
{
    if ((unsigned)owner >= HAL_OWNER_COUNT) return APP_ERR_ARG;
    app_status_t rc = app_hal_init();
    if (rc != APP_OK) return rc;
    if (slots[owner].open) return APP_ERR_BUSY;
    slots[owner].open = SLOT_ACTIVE;
    rc = hal_require(owner, mask);
    if (rc != APP_OK) slots[owner].open = slots[owner].mask ? SLOT_CLOSING : 0;
    return rc;
}

void hal_schedule(hal_owner_t owner, app_status_t (*update)(void))
{
    if (hal_active(owner)) slots[owner].update = update;
}

static void remove_page(hal_owner_t owner)
{
    for (unsigned i = 0; i < page_count; ++i) {
        if (pages[i] != owner) continue;
        --page_count;
        memmove(&pages[i], &pages[i + 1], (page_count - i) * sizeof pages[0]);
        break;
    }
}

app_status_t hal_blur(hal_owner_t owner)
{
    if (!hal_active(owner)) return APP_ERR_STATE;
    remove_page(owner);
    if (!in_update) (void)route_keys(0);
    return APP_OK;
}

app_status_t hal_focus(hal_owner_t owner)
{
    if (!hal_active(owner)) return APP_ERR_STATE;
    remove_page(owner);
    pages[page_count++] = owner;
    if (!in_update) (void)route_keys(0);
    return APP_OK;
}

int hal_focused(hal_owner_t owner)
{
    return hal_active(owner) && page_count && pages[page_count - 1U] == owner;
}

/* 与 app_segment_show_text 一致：负号/空格占一位，小数点可附在前一位。 */
static int copy_text(char dest[TEXT_SIZE], const char *src)
{
    unsigned digits = 0, i = 0;
    int point = 0;
    if (!src) return 0;
    for (; src[i]; ++i) {
        char c = src[i];
        if (i >= TEXT_SIZE - 1U) return 0;
        if (c != '.' && c != ' ' && c != '-' && (c < '0' || c > '9')) return 0;
        if (c != '.' || !digits || point) ++digits;
        point = c == '.';
        if (digits > 4) return 0;
    }
    memcpy(dest, src, i + 1U);
    return 1;
}

app_status_t hal_page(hal_owner_t owner, const char *text, app_rgb_color_t color)
{
    if (!hal_active(owner) || !(slots[owner].mask & HAL_SEG)) return APP_ERR_STATE;
    if ((unsigned)color > APP_RGB_WHITE || !copy_text(slots[owner].page, text)) return APP_ERR_ARG;
    slots[owner].color = color;
    return APP_OK;
}

app_status_t hal_alert(hal_owner_t owner, unsigned priority, const char *text, app_rgb_color_t color,
                       uint32_t hz, uint32_t on_ms, uint32_t gap_ms)
{
    if (!hal_active(owner)) return APP_ERR_STATE;
    uint32_t mask = slots[owner].mask;
    if ((text && !(mask & HAL_SEG)) || (color && !(mask & HAL_RGB)) || (hz && !(mask & HAL_BUZZ)))
        return APP_ERR_STATE;
    if (!priority || (unsigned)color > APP_RGB_WHITE || (hz && (hz < 100 || hz > 10000)) ||
        (gap_ms && !on_ms) || on_ms >= 0x7FFFFFFEU || gap_ms >= 0x7FFFFFFEU) return APP_ERR_ARG;
    alert_t next = {0};
    if (text && !copy_text(next.text, text)) return APP_ERR_ARG;
    next.priority = priority;
    next.has_text = text != NULL;
    next.light = (signal_t){color, on_ms, gap_ms};
    next.sound = (signal_t){hz, on_ms, gap_ms};
    slots[owner].alert = next;
    if (!in_update) (void)route_keys(0);
    return APP_OK;
}

app_status_t hal_alert_clear(hal_owner_t owner)
{
    if (!hal_active(owner)) return APP_ERR_STATE;
    slots[owner].alert.priority = 0;
    if (!in_update) (void)route_keys(0);
    return APP_OK;
}

static hal_owner_t top_alert(void)
{
    hal_owner_t best = HAL_OWNER_COUNT;
    unsigned priority = 0;
    for (unsigned i = 0; i < HAL_OWNER_COUNT; ++i)
        if (slots[i].alert.priority > priority ||
            (priority && slots[i].alert.priority == priority && !slots[best].alert.sound.value &&
             slots[i].alert.sound.value)) {
            priority = slots[i].alert.priority;
            best = (hal_owner_t)i;
        }
    return best;
}

static app_status_t set_signal(unsigned bit, signal_t desired, signal_t *previous)
{
    if (!desired.value || !desired.gap) desired.gap = desired.on = 0;
    if ((applied & bit) && desired.value == previous->value && desired.on == previous->on &&
        desired.gap == previous->gap) return APP_OK;
    app_status_t rc;
    if (bit == HAL_RGB) {
        if (desired.gap) {
            rc = app_rgb_set(APP_RGB_OFF);
            if (rc == APP_OK) rc = app_rgb_blink((app_rgb_color_t)desired.value, desired.on, desired.gap, UINT32_MAX);
        } else rc = app_rgb_set((app_rgb_color_t)desired.value);
    } else {
        rc = app_buzzer_stop();
        if (rc == APP_OK && desired.value) {
            if (desired.gap) rc = app_buzzer_repeat(desired.value, 50, desired.on, desired.gap, UINT32_MAX);
            else rc = app_buzzer_start(desired.value);
        }
    }
    if (rc == APP_OK) { *previous = desired; applied |= bit; }
    else applied &= ~bit; /* 即使目标不变，下一轮也要重试失败的写入。 */
    return rc;
}

/* 同级优先尚须鸣响的告警，再依固定模块顺序选择；同一时刻仅一个输出者。 */
static app_status_t render(void)
{
    uint32_t mask = device_mask(HAL_OWNER_COUNT, SLOT_ACTIVE);
    const char *text = "";
    signal_t light = {0}, sound = {0};
    if (page_count) {
        const client_t *s = &slots[pages[page_count - 1U]];
        if (s->mask & HAL_SEG) text = s->page;
        if (s->mask & HAL_RGB) light.value = s->color;
    }
    hal_owner_t top = top_alert();
    if (top != HAL_OWNER_COUNT) {
        const client_t *s = &slots[top];
        if (s->alert.has_text && (s->mask & HAL_SEG)) text = s->alert.text;
        if (s->mask & HAL_RGB) light = s->alert.light;
        if (s->mask & HAL_BUZZ) sound = s->alert.sound;
    }
    app_status_t rc = APP_OK;
    if ((mask & HAL_SEG) && (!(applied & HAL_SEG) || strcmp(text, shown))) {
        rc = app_segment_show_text(text);
        if (rc == APP_OK) { strcpy(shown, text); applied |= HAL_SEG; }
        else applied &= ~HAL_SEG;
    }
    if (mask & HAL_RGB) first_error(&rc, set_signal(HAL_RGB, light, &lit));
    if (mask & HAL_BUZZ) first_error(&rc, set_signal(HAL_BUZZ, sound, &sounding));
    return rc;
}

app_status_t hal_close(hal_owner_t owner)
{
    if ((unsigned)owner >= HAL_OWNER_COUNT || !on_thread() || !slots[owner].open) return APP_ERR_STATE;
    slots[owner].open = SLOT_CLOSING; /* 部分释放后只接受重试 de_init，停止调度该模块。 */
    slots[owner].update = NULL;
    remove_page(owner);
    slots[owner].alert.priority = 0;
    if (key_target == owner) key_target = HAL_OWNER_COUNT;
    app_status_t rc = release_mask(owner, VALID_BITS);
    if (rc == APP_OK) memset(&slots[owner], 0, sizeof slots[owner]);
    /* 其他页面的写入失败由下一次 update 重试，当前设备的释放结果独立返回。 */
    if (!in_update) { (void)route_keys(0); (void)render(); }
    return rc;
}

/* 切换时丢弃前一个页面的积压事件，之后发生的第一个短按也能送达新页面。 */
static app_status_t route_keys(int collect)
{
    hal_owner_t target = top_alert();
    if (target == HAL_OWNER_COUNT || !slots[target].alert.has_text)
        target = page_count ? pages[page_count - 1U] : HAL_OWNER_COUNT;
    if (target == HAL_OWNER_COUNT || !(slots[target].mask & HAL_KEYS)) target = HAL_OWNER_COUNT;
    if (!collect && target == key_target) return APP_OK;
    memset(key_events, 0, sizeof key_events);
    memset(key_down, 0, sizeof key_down);
    app_status_t result = APP_OK;
    if (device_mask(HAL_OWNER_COUNT, SLOT_ACTIVE) & HAL_KEYS) {
        for (unsigned i = 0; i < 4; ++i) {
            uint8_t events = 0, down = 0;
            app_key_id_t key = (app_key_id_t)(i + APP_KEY_1);
            app_status_t rc = app_key_take_events(key, &events);
            first_error(&result, rc);
            if (collect && rc == APP_OK && key_target == target) key_events[i] = events;
            rc = app_key_is_pressed(key, &down);
            first_error(&result, rc);
            if (collect && rc == APP_OK && key_target == target) key_down[i] = down;
        }
    }
    key_target = target;
    return result;
}

uint8_t hal_key_events(hal_owner_t owner, app_key_id_t key)
{
    if (!in_update || !hal_active(owner) || owner != key_target || key < APP_KEY_1 || key > APP_KEY_4) return 0;
    unsigned i = (unsigned)(key - APP_KEY_1);
    uint8_t events = key_events[i];
    key_events[i] = 0;
    return events;
}

int hal_key_down(hal_owner_t owner, app_key_id_t key)
{
    return in_update && hal_active(owner) && owner == key_target && key >= APP_KEY_1 && key <= APP_KEY_4
           && key_down[key - APP_KEY_1];
}

app_status_t hal_read(hal_owner_t owner, hal_sensor_t sensor, float *value)
{
    if ((unsigned)sensor >= HAL_SENSOR_COUNT || !value) return APP_ERR_ARG;
    if (!hal_active(owner) || !(slots[owner].mask & HAL_SENSOR_BIT(sensor))) return APP_ERR_STATE;
    if (!in_update || !samples[sensor].valid) {
        float v = 0;
        app_status_t rc = APP_ERR_STATE;
        switch (sensor) {
        case HAL_TEMP: rc = app_ntc_read_celsius(&v); break;
        case HAL_LUX: rc = app_light_read_lux(&v); break;
        case HAL_DBFS: rc = app_mic_read_dbfs(&v); break;
        case HAL_SPL: rc = app_mic_read_spl(&v); break;
        case HAL_HZ: rc = app_mic_read_frequency_hz(&v); break;
        case HAL_DISTANCE: {
            uint16_t mm = 0;
            rc = app_ultrasonic_read_mm(&mm);
            v = mm;
            break;
        }
        default: break;
        }
        samples[sensor].valid = 1;
        samples[sensor].result = rc;
        samples[sensor].value = v;
    }
    if (samples[sensor].result == APP_OK) *value = samples[sensor].value;
    return samples[sensor].result;
}

app_status_t app_hal_mic_calibrate_spl(float reference_db_spl)
{
    if (!on_thread() || !(device_mask(HAL_OWNER_COUNT, SLOT_ACTIVE) & MIC_BITS)) return APP_ERR_STATE;
    samples[HAL_SPL].valid = 0;
    return app_mic_calibrate_spl(reference_db_spl);
}

app_status_t app_hal_update(void)
{
    if (!on_thread()) return APP_ERR_STATE;
    if (in_update) return APP_ERR_BUSY;
    in_update = 1;
    for (unsigned i = 0; i < HAL_SENSOR_COUNT; ++i) samples[i].valid = 0;
    app_status_t rc = route_keys(1);
    for (unsigned i = 0; i < HAL_OWNER_COUNT; ++i)
        if (slots[i].open && slots[i].update) first_error(&rc, slots[i].update());
    first_error(&rc, render());
    first_error(&rc, route_keys(0));
    in_update = 0;
    return rc;
}

app_status_t app_hal_de_init(void)
{
    if (!on_thread()) return APP_ERR_STATE;
    if (in_update) return APP_ERR_BUSY;
    for (unsigned i = 0; i < HAL_OWNER_COUNT; ++i)
        if (slots[i].open) return APP_ERR_BUSY;
    key_target = HAL_OWNER_COUNT;
    applied = page_count = 0;
    service_thread = RT_NULL;
    return APP_OK;
}
