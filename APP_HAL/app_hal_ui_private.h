#ifndef APP_HAL_UI_PRIVATE_H
#define APP_HAL_UI_PRIVATE_H

#include "app_hal_internal.h"
#include "app_hal_config.h"
#include "app_key.h"

/* Fit the actual CH422G glass, counting the sign and leading zero as digits. */
static inline int ui_fixed_fits(int32_t v, uint8_t decimals)
{
    return decimals <= 3 && v >= -999 && v <= 9999 && (v >= 0 || decimals < 3);
}

static inline void ui_fixed_text(int32_t v, uint8_t decimals, char text[6])
{
    char reversed[5];
    unsigned n = 0, out = 0;
    unsigned magnitude = (unsigned)(v < 0 ? -v : v);
    do {
        reversed[n++] = (char)('0' + magnitude % 10U);
        magnitude /= 10U;
    } while (magnitude || n <= decimals);
    if (v < 0) text[out++] = '-';
    while (n) {
        if (n == decimals) text[out++] = '.';
        text[out++] = reversed[--n];
    }
    text[out] = '\0';
}

static inline void ui_pair_text(unsigned first, unsigned second, char text[6])
{
    text[0] = (char)('0' + first / 10U);
    text[1] = (char)('0' + first % 10U);
    text[2] = '.';
    text[3] = (char)('0' + second / 10U);
    text[4] = (char)('0' + second % 10U);
    text[5] = '\0';
}

typedef struct { int8_t direction; uint32_t last; } ui_repeat_t;

/* One delayed gesture per scan. Do not apply an accumulated burst after a slow sensor read. */
static inline int ui_key_delta(hal_owner_t owner, ui_repeat_t *repeat)
{
    if (!hal_focused(owner)) { repeat->direction = 0; return 0; }
    uint8_t up = hal_key_events(owner, APP_KEY_1), down = hal_key_events(owner, APP_KEY_2);
    uint8_t mask = APP_KEY_EVENT_CLICK | APP_KEY_EVENT_LONG_PRESS | APP_KEY_EVENT_DOUBLE_CLICK;
    if ((up & mask) && (down & mask)) { repeat->direction = 0; return 0; }
    int direction = (up & mask) ? 1 : (down & mask) ? -1 : 0;
    uint32_t now = hal_now_ms();
    if (direction) {
        uint8_t events = direction > 0 ? up : down;
        repeat->direction = (events & APP_KEY_EVENT_LONG_PRESS) ? direction : 0;
        repeat->last = now;
        return direction * ((events & APP_KEY_EVENT_DOUBLE_CLICK) ? 2 : 1);
    }
    if (!repeat->direction) return 0;
    if (!hal_key_down(owner, repeat->direction > 0 ? APP_KEY_1 : APP_KEY_2)) {
        repeat->direction = 0;
        return 0;
    }
    if (now - repeat->last < APP_HAL_KEY_REPEAT_MS) return 0;
    repeat->last = now;
    return repeat->direction;
}
#endif
