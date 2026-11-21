#include "veml7700.h"
#include "i2c.h"
#include <rtthread.h>

#define VEML7700_ADDR 0x10
#define ALS_GAIN_QUARTER (3U << 11)  /* 板载器件使用参考驱动默认的 1/4 倍增益。 */
static struct rt_mutex mutex;
static int last_config = -1;
static int light_ipc_init(void) { return rt_mutex_init(&mutex, "veml", RT_IPC_FLAG_PRIO); }
INIT_COMPONENT_EXPORT(light_ipc_init);

static int integration_code(uint16_t ms)
{
    switch (ms) {
    case 25: return 12;
    case 50: return 8;
    case 100: return 0;
    case 200: return 1;
    case 400: return 2;
    case 800: return 3;
    default: return -1;
    }
}

static int write_reg(uint8_t reg, uint16_t word)
{
    uint8_t data[2] = {(uint8_t)word, (uint8_t)(word >> 8)};
    return i2c_write_reg(VEML7700_ADDR, reg, data, 2);
}

int veml7700_raw(uint16_t integration_ms)
{
    int it = integration_code(integration_ms);
    if (it < 0) return -RT_EINVAL;
    int rc = rt_mutex_take(&mutex, RT_WAITING_FOREVER);
    if (rc != RT_EOK) return rc;
    int config = ALS_GAIN_QUARTER | (it << 6);
    int err = I2C_OK;
    if (last_config != config) {
        /* 先以当前配置休眠，再写新配置并唤醒。 */
        err = write_reg(0, (last_config < 0 ? 0 : last_config) | 1);
        if (err == I2C_OK && last_config < 0) err = write_reg(3, 0);
        if (err == I2C_OK) err = write_reg(0, config);
        if (err == I2C_OK) rt_thread_mdelay(integration_ms + integration_ms / 10 + 4);
    }
    uint8_t data[2];
    if (err == I2C_OK) err = i2c_read_reg(VEML7700_ADDR, 4, data, 2);
    last_config = err == I2C_OK ? config : -1;
    rc = err == I2C_OK ? data[0] | ((int)data[1] << 8) :
         err == I2C_ERR_TIMEOUT ? -RT_ETIMEOUT : -RT_EIO;
    rt_mutex_release(&mutex);
    return rc;
}

float veml7700_lux(uint16_t integration_ms)
{
    int raw = veml7700_raw(integration_ms);
    if (raw < 0) return (float)raw;
    if (raw == UINT16_MAX) return -RT_EIO;
    /* 1/4 倍增益、100 ms 积分时，每个计数对应 0.2304 lx。 */
    float x = raw * 0.2304f * (100.0f / integration_ms);
    if (x > 1000.0f)
        x = (((6.0135e-13f * x - 9.3924e-9f) * x + 8.1488e-5f) * x + 1.0023f) * x;
    return x;
}
