#ifndef APP_PD_H
#define APP_PD_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 板上 CH224A 的 CFG1 以 6.8 kΩ 下拉，硬件默认请求 9 V。
 * 固定电压只开放到 20 V：本板 VBUS 输入电容 C1 额定 25 V。 */
typedef enum {
    APP_PD_5V = 5, APP_PD_9V = 9, APP_PD_12V = 12, APP_PD_15V = 15, APP_PD_20V = 20
} app_pd_voltage_t;
enum {
    APP_PD_BC = 1, APP_PD_QC2 = 2, APP_PD_QC3 = 4, APP_PD_PD = 8, APP_PD_EPR = 16
};
typedef struct {
    uint8_t protocols;       /* 握手成功的协议位，见 APP_PD_* */
    uint16_t available_ma;   /* PD 当前档位允许电流；仅 APP_PD_PD 置位时有效 */
} app_pd_status_t;

/* 接管 CH224A 的 I2C 通信，只读取状态，保留现有电压合同。 */
app_status_t app_pd_init(void);
/* 获取当前协议与协商电流；这是供电能力，负载实际电流需另行测量。 */
app_status_t app_pd_read_status(app_pd_status_t *status);
/* 请求指定电压，成功表示已发送，实际电压取决于适配器和线缆。 */
app_status_t app_pd_request_voltage(app_pd_voltage_t volts);
/* 释放 I2C 引用，当前电源合同持续有效。 */
app_status_t app_pd_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
