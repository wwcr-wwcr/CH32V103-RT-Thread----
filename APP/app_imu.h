#ifndef APP_IMU_H
#define APP_IMU_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* P4 插座所配 QMI8658 六轴模块，坐标系按传感器本体 X/Y/Z。 */
typedef struct {
    float accel_g[3];
    float gyro_dps[3];
    float die_temperature_c;
    uint32_t sample_index; /* 芯片的 24 位采样计数，满后回绕 */
} app_imu_sample_t;

/* 在 0x6B、0x6A 校验 WHO_AM_I，通过后配置 ±4 g、±512 °/s。 */
app_status_t app_imu_init(void);
/* 读取同一次采样的六轴数据。无新样本或传输期间芯片刷新时返回 BUSY，
 * 可在下一次调度时重试；失败时 sample 保留原值。 */
app_status_t app_imu_read(app_imu_sample_t *sample);
/* 停用加速度计、陀螺仪并释放共享总线引用。 */
app_status_t app_imu_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
