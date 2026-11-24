#ifndef VEML7700_H
#define VEML7700_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
/* 在线程中调用。积分时间支持 25、50、100、200、400、800 ms。
 * 首次读取或改变积分时间时等待新结果；相同积分时间读取最近一次转换。负数为错误。
 * 例：veml7700_lux(50); */
float veml7700_lux(uint16_t integration_ms);
int veml7700_raw(uint16_t integration_ms);

#ifdef __cplusplus
}
#endif
#endif
