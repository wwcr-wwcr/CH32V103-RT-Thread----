#include "app_pd.h"
#include "app_sensor_bus.h"

/* 套件固件使用 0x22。若所配 CH224A 的订货版本使用 0x23，可在工程中覆盖。 */
#ifndef APP_PD_I2C_ADDRESS
#define APP_PD_I2C_ADDRESS 0x22U
#endif
#define PD_PROTOCOL 0x09U
#define PD_VOLTAGE  0x0AU
#define PD_CURRENT  0x50U

app_status_t app_pd_init(void)
{
    uint8_t value;
    app_status_t rc = app_sensor_bus_begin(SENSOR_PD, SENSOR_OPEN);
    if (rc != APP_OK) return rc;
    rc = app_sensor_read(APP_PD_I2C_ADDRESS, PD_PROTOCOL, &value, 1);
    return app_sensor_bus_end(SENSOR_PD, SENSOR_OPEN, rc);
}

app_status_t app_pd_read_status(app_pd_status_t *status)
{
    uint8_t flags, current = 0;
    if (!status) return APP_ERR_ARG;
    app_status_t rc = app_sensor_bus_begin(SENSOR_PD, SENSOR_USE);
    if (rc != APP_OK) return rc;
    rc = app_sensor_read(APP_PD_I2C_ADDRESS, PD_PROTOCOL, &flags, 1);
    if (rc == APP_OK && (flags & APP_PD_PD))
        rc = app_sensor_read(APP_PD_I2C_ADDRESS, PD_CURRENT, &current, 1);
    if (rc == APP_OK) {
        status->protocols = flags & 0x1FU;
        status->available_ma = (uint16_t)current * 50U;
    }
    return app_sensor_bus_end(SENSOR_PD, SENSOR_USE, rc);
}

app_status_t app_pd_request_voltage(app_pd_voltage_t volts)
{
    static const uint8_t gears[] = {5, 9, 12, 15, 20};
    size_t gear = 0;
    while (gear < sizeof(gears) && gears[gear] != volts) ++gear;
    if (gear == sizeof(gears)) return APP_ERR_ARG;
    app_status_t rc = app_sensor_bus_begin(SENSOR_PD, SENSOR_USE);
    if (rc != APP_OK) return rc;
    rc = app_sensor_write8(APP_PD_I2C_ADDRESS, PD_VOLTAGE, (uint8_t)gear);
    return app_sensor_bus_end(SENSOR_PD, SENSOR_USE, rc);
}

app_status_t app_pd_de_init(void)
{
    app_status_t rc = app_sensor_bus_begin(SENSOR_PD, SENSOR_CLOSE);
    if (rc != APP_OK) return rc;
    return app_sensor_bus_end(SENSOR_PD, SENSOR_CLOSE, APP_OK);
}
