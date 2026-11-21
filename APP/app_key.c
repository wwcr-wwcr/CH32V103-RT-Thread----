#include "app_key.h"
#include "app_internal.h"
#include <string.h>

#define DEBOUNCE_MS 20
#define DOUBLE_CLICK_MS 300
#define LONG_PRESS_MS 700
#define ALL_GESTURES (APP_KEY_EVENT_CLICK | APP_KEY_EVENT_DOUBLE_CLICK | APP_KEY_EVENT_LONG_PRESS)
static const bsp_pin_t pins[] = {
    BSP_PIN_BUTTON_1, BSP_PIN_BUTTON_2, BSP_PIN_BUTTON_3, BSP_PIN_BUTTON_4
};
#define KEY_COUNT (sizeof pins / sizeof pins[0])
typedef struct {
    rt_tick_t changed_at, pressed_at, released_at;
    uint8_t raw, pressed, ignore, long_sent, waiting, second, events;
} key_state_t;
static key_state_t keys[KEY_COUNT];
static app_device_t device;
static struct rt_timer scan_timer;
/* 菜单常驻，普通 APP（包括 APP_HAL）取得按键时临时拥有全部手势。 */
enum { KEY_APP = 1, KEY_NAVIGATION = 2 };
static uint8_t clients;

static void discard_gestures(void)
{
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        uint8_t raw = keys[i].raw;
        keys[i] = (key_state_t){ .raw = raw, .pressed = raw, .ignore = raw };
    }
}

static void scan_tick(void *unused)
{
    (void)unused;
    (void)app_key_scan();
}

static app_status_t open_keys(uint8_t client)
{
    app_status_t result = app_try_lock(&device.lock);
    if (result != APP_OK) return result;
    if (clients) {
        if (clients & client) result = APP_ERR_BUSY;
        else {
            if (client == KEY_APP) discard_gestures();
            clients |= client;
        }
        app_device_leave(&device, 1);
        return result;
    }
    size_t acquired = 0;
    memset(keys, 0, sizeof keys);
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        result = app_from_bsp(bsp_gpio_init(pins[i], GPIO_Mode_IPU, 1));
        if (result != APP_OK) break;
        acquired++;
        uint8_t high;
        result = app_from_bsp(bsp_gpio_read(pins[i], &high));
        if (result != APP_OK) break;
        keys[i].ignore = keys[i].raw = keys[i].pressed = !high;
    }
    if (result == APP_OK) {
        rt_tick_t period = rt_tick_from_millisecond(5);
        rt_timer_init(&scan_timer, "app_keys", scan_tick, RT_NULL, period ? period : 1,
                      RT_TIMER_FLAG_PERIODIC | RT_TIMER_FLAG_HARD_TIMER);
        if (rt_timer_start(&scan_timer) != RT_EOK) {
            rt_timer_detach(&scan_timer);
            result = APP_ERR_IO;
        }
    }
    if (result != APP_OK)
        while (acquired) bsp_gpio_de_init(pins[--acquired]);
    else clients = client;
    app_device_leave(&device, result == APP_OK);
    return result;
}

static app_status_t close_keys(uint8_t client)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    if (!(clients & client)) {
        app_device_leave(&device, 1);
        return APP_ERR_STATE;
    }
    clients &= (uint8_t)~client;
    if (clients) {
        if (client == KEY_APP) discard_gestures();
        app_device_leave(&device, 1);
        return APP_OK;
    }
    rt_timer_detach(&scan_timer);
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        app_status_t status = app_from_bsp(bsp_gpio_de_init(pins[i]));
        if (result == APP_OK) result = status;
    }
    memset(keys, 0, sizeof keys);
    app_device_leave(&device, 0);
    return result;
}

app_status_t app_key_init(void) { return open_keys(KEY_APP); }
app_status_t app_key_de_init(void) { return close_keys(KEY_APP); }
app_status_t app_key_navigation_init(void) { return open_keys(KEY_NAVIGATION); }
app_status_t app_key_navigation_de_init(void) { return close_keys(KEY_NAVIGATION); }

static void update(key_state_t *k, uint8_t pressed, rt_tick_t now)
{
    if (k->raw != pressed) {
        k->raw = pressed;
        k->changed_at = now;
    }
    rt_tick_t window = rt_tick_from_millisecond(DOUBLE_CLICK_MS);
    /* 在双击窗口末端刚刚按下的动作仍有一次消抖时间。 */
    if (k->waiting && now - k->released_at >= window &&
        (!k->raw || k->changed_at - k->released_at >= window)) {
        k->events |= APP_KEY_EVENT_CLICK;
        k->waiting = 0;
    }
    if (k->raw != k->pressed && now - k->changed_at >= rt_tick_from_millisecond(DEBOUNCE_MS)) {
        k->pressed = k->raw;
        if (k->pressed) {
            k->pressed_at = now;
            k->long_sent = 0;
            k->second = k->waiting;
            k->waiting = 0;
        } else if (k->ignore) {
            k->ignore = 0;
        } else if (!k->long_sent) {
            if (k->second) k->events |= APP_KEY_EVENT_DOUBLE_CLICK;
            else {
                k->waiting = 1;
                k->released_at = now;
            }
        }
    }
    /* 若满长按门限的这一拍刚好读到松手，仍按首个松手采样的时间判断。
     * 松手消抖期间经过的时间不计入按住时长。 */
    rt_tick_t held_until = k->raw ? now : k->changed_at;
    if (k->pressed && !k->ignore && !k->long_sent &&
        held_until - k->pressed_at >= rt_tick_from_millisecond(LONG_PRESS_MS)) {
        k->events |= APP_KEY_EVENT_LONG_PRESS;
        if (k->second) k->events |= APP_KEY_EVENT_CLICK;
        k->long_sent = 1;
    }
}

app_status_t app_key_scan(void)
{
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    rt_tick_t now = rt_tick_get();
    for (size_t i = 0; i < KEY_COUNT; ++i) {
        uint8_t high;
        result = app_from_bsp(bsp_gpio_read(pins[i], &high));
        if (result != APP_OK) break;
        update(&keys[i], !high, now);
    }
    app_device_leave(&device, 1);
    return result;
}

app_status_t app_key_is_pressed(app_key_id_t key, uint8_t *pressed)
{
    if (key < APP_KEY_1 || key > APP_KEY_4 || !pressed) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    if (clients & KEY_APP) *pressed = keys[key - APP_KEY_1].pressed;
    else result = APP_ERR_STATE;
    app_device_leave(&device, 1);
    return result;
}

static app_status_t take_events(app_key_id_t key, uint8_t mask, uint8_t *value, uint8_t client)
{
    if (key < APP_KEY_1 || key > APP_KEY_4 || !value) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&device, 1);
    if (result != APP_OK) return result;
    if (!(clients & client)) {
        app_device_leave(&device, 1);
        return APP_ERR_STATE;
    }
    if (client == KEY_NAVIGATION && (clients & KEY_APP)) {
        *value = 0;
        app_device_leave(&device, 1);
        return APP_OK;
    }
    key_state_t *state = &keys[key - APP_KEY_1];
    uint8_t pending = state->events & mask;
    state->events &= (uint8_t)~mask;
    *value = mask == ALL_GESTURES ? pending : (uint8_t)(pending != 0);
    app_device_leave(&device, 1);
    return APP_OK;
}

app_status_t app_key_take_events(app_key_id_t key, uint8_t *events)
{
    return take_events(key, ALL_GESTURES, events, KEY_APP);
}

app_status_t app_key_navigation_take_events(app_key_id_t key, uint8_t *events)
{
    return take_events(key, ALL_GESTURES, events, KEY_NAVIGATION);
}

app_status_t app_key_take_short_press(app_key_id_t key, uint8_t *detected)
{
    return take_events(key, APP_KEY_EVENT_CLICK, detected, KEY_APP);
}

app_status_t app_key_take_double_click(app_key_id_t key, uint8_t *detected)
{
    return take_events(key, APP_KEY_EVENT_DOUBLE_CLICK, detected, KEY_APP);
}

app_status_t app_key_take_long_press(app_key_id_t key, uint8_t *detected)
{
    return take_events(key, APP_KEY_EVENT_LONG_PRESS, detected, KEY_APP);
}
