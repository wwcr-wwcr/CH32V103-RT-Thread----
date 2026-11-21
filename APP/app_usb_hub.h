#ifndef APP_USB_HUB_H
#define APP_USB_HUB_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define APP_USB_HUB_PORTS 4U
typedef struct {
    uint8_t connected;     /* 1 有下游设备接入。 */
    uint8_t enabled;       /* 1 端口已获准进行 USB 数据通信。 */
    uint8_t powered;       /* 集线器报告的逻辑 PORT_POWER 位。 */
    uint8_t over_current;  /* 集线器报告的过流状态。 */
} app_usb_hub_port_t;

/* 在线程中启动 USBHS 主机并枚举板载 CH334R。无参数。占用 PB6/PB7，成功后可访问各端口。 */
app_status_t app_usb_hub_init(void);
/* 读取端口状态。port 为集线器逻辑端口号 1~4，state 为结果地址，成功时更新。在线程中调用。 */
app_status_t app_usb_hub_read_port(uint8_t port, app_usb_hub_port_t *state);
/* 请求集线器调整逻辑端口电源。port 为 1~4，on 为 0/1。开启时等待器件报告的上电稳定时间。
 * P7/P8 的 VBUS 硬接板上 5 V。成组模式会影响全组，常供电模式下关闭返回 UNSUPPORTED。 */
app_status_t app_usb_hub_set_port_power(uint8_t port, uint8_t on);
/* 在线程中撤销集线器配置，关闭主机并归还 PB6/PB7。无参数。
 * 集线器失联时仍清理本地主机，返回总线错误。主机资源繁忙时可重试解初始化。 */
app_status_t app_usb_hub_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
