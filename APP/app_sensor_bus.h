#ifndef APP_SENSOR_BUS_H
#define APP_SENSOR_BUS_H
#include "app_types.h"

/* APP 私有。begin 成功后持有 I2C1，必须用相同的设备、操作调用 end。 */
typedef enum { SENSOR_LIGHT = 1, SENSOR_IMU = 2, SENSOR_PD = 4 } sensor_client_t;
typedef enum { SENSOR_OPEN, SENSOR_USE, SENSOR_CLOSE } sensor_operation_t;
app_status_t app_sensor_bus_begin(sensor_client_t client, sensor_operation_t op);
app_status_t app_sensor_bus_end(sensor_client_t client, sensor_operation_t op, app_status_t result);
app_status_t app_sensor_read(uint8_t address, uint8_t reg, uint8_t *data, size_t length);
app_status_t app_sensor_write8(uint8_t address, uint8_t reg, uint8_t value);
app_status_t app_sensor_write16(uint8_t address, uint8_t reg, uint16_t value);
#endif
