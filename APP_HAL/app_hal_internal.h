#ifndef APP_HAL_INTERNAL_H
#define APP_HAL_INTERNAL_H

#include "app_hal_config.h"
#include "app_key.h"
#include "app_rgb.h"

typedef enum {
    HAL_METER, HAL_TIMER, HAL_CLOCK, HAL_COUNTER, HAL_PARAM, HAL_ALARM,
    HAL_FAN, HAL_PROXIMITY, HAL_NOISE, HAL_RPM, HAL_TILT, HAL_WAVE_INPUT,
    HAL_WAVE_OUTPUT, HAL_WAVE_LOOPBACK, HAL_OWNER_COUNT
} hal_owner_t;
typedef enum { HAL_TEMP, HAL_LUX, HAL_DBFS, HAL_SPL, HAL_HZ, HAL_DISTANCE, HAL_SENSOR_COUNT } hal_sensor_t;
#define HAL_SENSOR_BIT(s) (1UL << (s))
enum {
    HAL_SEG = 1U << 6, HAL_RGB = 1U << 7, HAL_BUZZ = 1U << 8, HAL_KEYS = 1U << 9,
    HAL_RTC = 1U << 10, HAL_FAN_DEV = 1U << 11, HAL_ENCODER = 1U << 12, HAL_IMU = 1U << 13
};

uint32_t hal_now_ms(void);
int hal_active(hal_owner_t owner);
app_status_t hal_open(hal_owner_t owner, uint32_t mask);
app_status_t hal_require(hal_owner_t owner, uint32_t mask);
app_status_t hal_drop(hal_owner_t owner, uint32_t mask);
app_status_t hal_close(hal_owner_t owner);
void hal_schedule(hal_owner_t owner, app_status_t (*update)(void));
app_status_t hal_read(hal_owner_t owner, hal_sensor_t sensor, float *value);
app_status_t hal_focus(hal_owner_t owner);
app_status_t hal_blur(hal_owner_t owner);
int hal_focused(hal_owner_t owner);
app_status_t hal_page(hal_owner_t owner, const char *text, app_rgb_color_t color);
app_status_t hal_alert(hal_owner_t owner, unsigned priority, const char *text,
                       app_rgb_color_t color, uint32_t hz, uint32_t on_ms, uint32_t gap_ms);
app_status_t hal_alert_clear(hal_owner_t owner);
uint8_t hal_key_events(hal_owner_t owner, app_key_id_t key);
int hal_key_down(hal_owner_t owner, app_key_id_t key);
#endif
