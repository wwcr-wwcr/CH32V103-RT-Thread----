#ifndef APP_ANALOG_INPUT_H
#define APP_ANALOG_INPUT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 定频外部输入；instance 为 ADC1/2 的编号，channel=0..15。 */
app_status_t app_analog_input_init(uint8_t instance, uint8_t channel);
app_status_t app_analog_input_capture(uint32_t rate_hz, uint16_t *samples, size_t count);
app_status_t app_analog_input_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
