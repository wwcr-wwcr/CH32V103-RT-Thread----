#include "app_internal.h"
#include "app_light.h"
#include "app_sensor_bus.h"

#define LIGHT_ADDRESS  0x10U
#define LIGHT_CONFIG   0x1000U  /* 增益 1/8、100 ms、上电、禁用中断 */
#define LIGHT_READY_MS 110U
static rt_tick_t started;
static uint8_t ready;

app_status_t app_light_init(void)
{
    app_status_t rc = app_sensor_bus_begin(SENSOR_LIGHT, SENSOR_OPEN);
    if (rc != APP_OK) return rc;
    rc = app_sensor_write16(LIGHT_ADDRESS, 0x03, 0); /* 关闭省电间隔 */
    if (rc == APP_OK) rc = app_sensor_write16(LIGHT_ADDRESS, 0x00, LIGHT_CONFIG);
    if (rc == APP_OK) {
        started = rt_tick_get();
        ready = 0;
    }
    return app_sensor_bus_end(SENSOR_LIGHT, SENSOR_OPEN, rc);
}

app_status_t app_light_read_raw(uint16_t *raw)
{
    uint8_t data[2];
    if (!raw) return APP_ERR_ARG;
    app_status_t rc = app_sensor_bus_begin(SENSOR_LIGHT, SENSOR_USE);
    if (rc != APP_OK) return rc;
    if (!ready && (rt_tick_t)(rt_tick_get() - started) >= rt_tick_from_millisecond(LIGHT_READY_MS) + 1U)
        ready = 1;
    rc = ready ? app_sensor_read(LIGHT_ADDRESS, 0x04, data, sizeof(data)) : APP_ERR_BUSY;
    if (rc == APP_OK) *raw = (uint16_t)data[0] | ((uint16_t)data[1] << 8);
    return app_sensor_bus_end(SENSOR_LIGHT, SENSOR_USE, rc);
}

app_status_t app_light_read_lux(float *lux)
{
    uint16_t raw = UINT16_MAX;
    if (!lux) return APP_ERR_ARG;
    app_status_t rc = app_light_read_raw(&raw);
    if (rc != APP_OK) return rc;
    if (raw == UINT16_MAX) return APP_ERR_IO;
    float x = raw * 0.5376f;
    /* Vishay AN 84323：低增益下，1000 lx 以上使用四次多项式修正。 */
    if (x > 1000.0f)
        x = (((6.0135e-13f * x - 9.3924e-9f) * x + 8.1488e-5f) * x + 1.0023f) * x;
    if (x > 140000.0f) return APP_ERR_IO; /* 超出器件标称照度范围 */
    *lux = x;
    return APP_OK;
}

app_status_t app_light_de_init(void)
{
    app_status_t rc = app_sensor_bus_begin(SENSOR_LIGHT, SENSOR_CLOSE);
    if (rc != APP_OK) return rc;
    rc = app_sensor_write16(LIGHT_ADDRESS, 0x00, LIGHT_CONFIG | 1U);
    return app_sensor_bus_end(SENSOR_LIGHT, SENSOR_CLOSE, rc);
}
