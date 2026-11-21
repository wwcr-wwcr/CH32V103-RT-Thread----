#ifndef APP_HAL_MONITOR_PRIVATE_H
#define APP_HAL_MONITOR_PRIVATE_H
#include "app_hal_ui_private.h"
#include <math.h>
#include <string.h>

typedef struct { uint32_t last; uint8_t started; } monitor_poll_t;

static inline int monitor_due(monitor_poll_t *poll, uint32_t now, uint32_t period)
{
    if (poll->started && now - poll->last < period) return 0;
    poll->last = now;
    poll->started = 1;
    return 1;
}

typedef struct {
    float trip, reset;
    uint32_t confirm_ms, since, last;
    uint8_t active, pending, acknowledged;
} monitor_rule_t;

static inline int monitor_valid_rule(float trip, float reset, uint32_t confirm_ms)
{
    return isfinite(trip) && isfinite(reset) && trip != reset && confirm_ms <= INT32_MAX;
}

static inline void monitor_observe(monitor_rule_t *rule, uint32_t now, float value)
{
    int high = rule->trip > rule->reset;
    if (rule->pending && now - rule->last > APP_HAL_SENSOR_MAX_GAP_MS) rule->pending = 0;
    rule->last = now;
    if (rule->active) {
        if (high ? value <= rule->reset : value >= rule->reset) {
            rule->active = 0;
            rule->acknowledged = 0;
        }
    } else if (high ? value >= rule->trip : value <= rule->trip) {
        if (!rule->pending) {
            rule->pending = 1;
            rule->since = now;
        }
        if (now - rule->since >= rule->confirm_ms) {
            rule->pending = 0;
            rule->active = 1;
        }
    } else rule->pending = 0;
}

/* Sensor pages use at most one decimal place. Overflow and invalid samples have an explicit display. */
static inline void monitor_text(float value, uint8_t decimals, char text[6])
{
    float scaled = decimals ? value * 10.0f : value;
    if (!(scaled > -999.5f && scaled < 9999.5f)) {
        strcpy(text, "----");
        return;
    }
    int32_t rounded = (int32_t)(scaled < 0 ? scaled - 0.5f : scaled + 0.5f);
    ui_fixed_text(rounded, decimals, text);
}
#endif
