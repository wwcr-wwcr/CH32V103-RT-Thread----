#include "app_internal.h"
#include "app_sensor_bus.h"

static volatile uint8_t bus_lock;
static uint8_t clients;
static uint8_t bus_open;

app_status_t app_sensor_bus_begin(sensor_client_t client, sensor_operation_t op)
{
    app_status_t rc = app_try_lock(&bus_lock);
    if (rc != APP_OK) return rc;
    if (op == SENSOR_OPEN) {
        if (clients & client) rc = APP_ERR_BUSY;
        else if (!bus_open) {
            rc = app_from_bsp(bsp_i2c_init(I2C1, 100000U, 1));
            if (rc == APP_OK) bus_open = 1;
        }
    } else if (!(clients & client)) rc = APP_ERR_STATE;
    if (rc != APP_OK) app_unlock(&bus_lock);
    return rc;
}

app_status_t app_sensor_bus_end(sensor_client_t client, sensor_operation_t op, app_status_t rc)
{
    if (op == SENSOR_OPEN && rc == APP_OK) clients |= (uint8_t)client;
    /* 拔掉模块后关机命令会失败，仍须释放主机侧引用。 */
    if (op == SENSOR_CLOSE) clients &= (uint8_t)~client;
    if (!clients && bus_open) {
        app_status_t close_rc = app_from_bsp(bsp_i2c_de_init(I2C1));
        if (close_rc == APP_OK) bus_open = 0;
        else {
            if (op == SENSOR_CLOSE) clients |= (uint8_t)client;
            if (rc == APP_OK) rc = close_rc;
        }
    }
    app_unlock(&bus_lock);
    return rc;
}

app_status_t app_sensor_read(uint8_t address, uint8_t reg, uint8_t *data, size_t length)
{
    return app_from_bsp(bsp_i2c_transfer(I2C1, address, &reg, 1, data, length));
}
app_status_t app_sensor_write8(uint8_t address, uint8_t reg, uint8_t value)
{
    uint8_t tx[] = {reg, value};
    return app_from_bsp(bsp_i2c_transfer(I2C1, address, tx, sizeof(tx), NULL, 0));
}
app_status_t app_sensor_write16(uint8_t address, uint8_t reg, uint16_t value)
{
    uint8_t tx[] = {reg, (uint8_t)value, (uint8_t)(value >> 8)};
    return app_from_bsp(bsp_i2c_transfer(I2C1, address, tx, sizeof(tx), NULL, 0));
}
