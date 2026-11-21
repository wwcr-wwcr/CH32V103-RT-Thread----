#ifndef APP_BLE_H
#define APP_BLE_H

#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 板载蓝牙串口透传。默认 115200/8N1，与模块配置保持一致。 */
#ifndef APP_BLE_BAUD
#define APP_BLE_BAUD 115200UL
#endif
app_status_t app_ble_init(void);
/* 停止接收，清除缓存并释放蓝牙串口。 */
app_status_t app_ble_de_init(void);
/* 发送任意二进制数据。sent 可空，出错时可用其检查已发送长度。 */
app_status_t app_ble_send(const void *data, size_t length, size_t *sent);
/* 非阻塞读取，received 必填。空队列为 BUSY，溢出或帧错误为 IO。 */
app_status_t app_ble_receive(void *dst, size_t capacity, size_t *received);

#ifdef __cplusplus
}
#endif
#endif
