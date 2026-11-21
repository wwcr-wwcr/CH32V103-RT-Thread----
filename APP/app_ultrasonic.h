#ifndef APP_ULTRASONIC_H
#define APP_ULTRASONIC_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 P5 的 3.3 V CS100A 超声波接口。无参数。TRIG 保持低电平，等待第一次测量。 */
app_status_t app_ultrasonic_init(void);
/* 触发一次并测距。distance_mm 为结果地址，单位 mm，成功时写入。按声速 340 m/s 换算。
 * 线程中调用；两次触发间至少 50 ms，回波超时 40 ms。没有有效回波时返回错误并保留旧值。 */
app_status_t app_ultrasonic_read_mm(uint16_t *distance_mm);
/* 停止输入捕获并释放 PC10、PC11 和 TIM10。无参数。测量期间调用返回 BUSY。 */
app_status_t app_ultrasonic_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
