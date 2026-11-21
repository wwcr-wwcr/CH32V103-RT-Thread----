#include "app_imu.h"
#include "app_sensor_bus.h"
#include <string.h>

#define IMU_WHO_AM_I  0x00U
#define IMU_CTRL1     0x02U
#define IMU_CTRL2     0x03U
#define IMU_CTRL3     0x04U
#define IMU_CTRL7     0x08U
#define IMU_STATUS0   0x2EU
#define IMU_TIMESTAMP 0x30U
static uint8_t address;

static app_status_t find_imu(void)
{
    const uint8_t choices[] = {0x6B, 0x6A};
    uint8_t id;
    for (size_t i = 0; i < sizeof(choices); ++i) {
        app_status_t rc = app_sensor_read(choices[i], IMU_WHO_AM_I, &id, 1);
        if (rc == APP_OK && id == 0x05U) {
            address = choices[i];
            return APP_OK;
        }
        if (rc != APP_OK && rc != APP_ERR_IO) return rc;
    }
    return APP_ERR_NOT_FOUND;
}

app_status_t app_imu_init(void)
{
    app_status_t rc = app_sensor_bus_begin(SENSOR_IMU, SENSOR_OPEN);
    if (rc != APP_OK) return rc;
    rc = find_imu();
    if (rc == APP_OK) {
        rc = app_sensor_write8(address, IMU_CTRL7, 0); /* 先停测量再改量程 */
        if (rc == APP_OK) rc = app_sensor_write8(address, IMU_CTRL1, 0x40); /* 连读、小端 */
        if (rc == APP_OK) rc = app_sensor_write8(address, IMU_CTRL2, 0x16); /* ±4 g，ODR=6 */
        if (rc == APP_OK) rc = app_sensor_write8(address, IMU_CTRL3, 0x46); /* ±512 °/s */
        if (rc == APP_OK) rc = app_sensor_write8(address, IMU_CTRL7, 0x03);
        if (rc != APP_OK) (void)app_sensor_write8(address, IMU_CTRL7, 0);
    }
    return app_sensor_bus_end(SENSOR_IMU, SENSOR_OPEN, rc);
}

static int16_t signed_le16(const uint8_t *p)
{
    return (int16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

app_status_t app_imu_read(app_imu_sample_t *sample)
{
    uint8_t flags, frame[17], after[3];
    if (!sample) return APP_ERR_ARG;
    app_status_t rc = app_sensor_bus_begin(SENSOR_IMU, SENSOR_USE);
    if (rc != APP_OK) return rc;
    rc = app_sensor_read(address, IMU_STATUS0, &flags, 1);
    if (rc == APP_OK && (flags & 3U) != 3U) rc = APP_ERR_BUSY;
    /* 0x30 起连续读计数、温度、加速度、角速度，再验证传输期间无新采样。 */
    if (rc == APP_OK) rc = app_sensor_read(address, IMU_TIMESTAMP, frame, sizeof(frame));
    if (rc == APP_OK) rc = app_sensor_read(address, IMU_TIMESTAMP, after, sizeof(after));
    if (rc == APP_OK && memcmp(frame, after, sizeof(after)) != 0) rc = APP_ERR_BUSY;
    if (rc == APP_OK) {
        sample->sample_index = (uint32_t)frame[0] | ((uint32_t)frame[1] << 8)
                              | ((uint32_t)frame[2] << 16);
        sample->die_temperature_c = signed_le16(frame + 3) / 256.0f;
        for (size_t axis = 0; axis < 3; ++axis) {
            sample->accel_g[axis] = signed_le16(frame + 5 + axis * 2) / 8192.0f;
            sample->gyro_dps[axis] = signed_le16(frame + 11 + axis * 2) / 64.0f;
        }
    }
    return app_sensor_bus_end(SENSOR_IMU, SENSOR_USE, rc);
}

app_status_t app_imu_de_init(void)
{
    app_status_t rc = app_sensor_bus_begin(SENSOR_IMU, SENSOR_CLOSE);
    if (rc != APP_OK) return rc;
    rc = app_sensor_write8(address, IMU_CTRL7, 0);
    return app_sensor_bus_end(SENSOR_IMU, SENSOR_CLOSE, rc);
}
