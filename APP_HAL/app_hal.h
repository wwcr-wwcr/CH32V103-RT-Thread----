#ifndef APP_HAL_H
#define APP_HAL_H
#include "app_hal_config.h"
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 由一个 RT-Thread 应用线程持有整套联动。init 可显式调用，也可由首个模块自动建立。
 * 在同一线程内调用配置、启动、update 和释放；不要再用 APP 直接写已被 HAL 接管的设备。 */
app_status_t app_hal_init(void);
/* 在应用循环调用，建议间隔 APP_HAL_SERVICE_MS。所有已启用的模块都会获得调度；
 * 即使某个传感器尚未就绪或出错，其他模块和报警输出仍刷新。返回本轮首个错误，BUSY 可重试。 */
app_status_t app_hal_update(void);
/* 所有功能模块 de_init 后释放调度器；仍有活动模块时返回 BUSY。 */
app_status_t app_hal_de_init(void);
/* 麦克风页面或噪声功能已经启用后，在已知声压的稳定声场中调用一次。 */
app_status_t app_hal_mic_calibrate_spl(float reference_db_spl);
#ifdef __cplusplus
}
#endif

#include "app_hal_meter.h"
#include "app_hal_timer.h"
#include "app_hal_clock.h"
#include "app_hal_counter.h"
#include "app_hal_param.h"
#include "app_hal_alarm.h"
#include "app_hal_fan.h"
#include "app_hal_proximity.h"
#include "app_hal_noise.h"
#include "app_hal_rpm.h"
#include "app_hal_tilt.h"
#include "app_hal_wave_input.h"
#include "app_hal_wave_output.h"
#include "app_hal_wave_loopback.h"
#endif
