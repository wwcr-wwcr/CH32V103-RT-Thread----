#ifndef APP_ANALOG_OUTPUT_H
#define APP_ANALOG_OUTPUT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif
/* DAC1/2。play 的 12 位码表在 stop 前必须一直有效；非循环表的末值为结束电平。 */
app_status_t app_analog_output_init(uint8_t channel);
app_status_t app_analog_output_play(const uint16_t *table, uint16_t count, uint32_t sample_hz,
                                    uint8_t circular, uint16_t idle_code, float *actual_hz);
app_status_t app_analog_output_running(uint8_t *running);
app_status_t app_analog_output_stop(void);
app_status_t app_analog_output_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
