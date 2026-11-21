#include "app_ble.h"
#include "app_internal.h"

static app_device_t ble;
static uint8_t ring[257]; /* 最多暂存 256 字节 */
app_status_t app_ble_init(void)
{
    app_status_t rc = app_device_enter(&ble, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_init(USART3, APP_BLE_BAUD, 0));
    if (rc == APP_OK) {
        rc = app_from_bsp(bsp_uart_buffer_rx(USART3, ring, sizeof ring));
        if (rc != APP_OK) bsp_uart_de_init(USART3);
    }
    app_device_leave(&ble, rc == APP_OK);
    return rc;
}
app_status_t app_ble_de_init(void)
{
    app_status_t rc = app_device_enter(&ble, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_de_init(USART3));
    app_device_leave(&ble, rc != APP_OK);
    return rc;
}
app_status_t app_ble_send(const void *data, size_t length, size_t *sent)
{
    if (sent) *sent = 0;
    app_status_t rc = app_device_enter(&ble, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_write(USART3, data, length, sent));
    app_device_leave(&ble, 1);
    return rc;
}
app_status_t app_ble_receive(void *dst, size_t capacity, size_t *received)
{
    if (received) *received = 0;
    app_status_t rc = app_device_enter(&ble, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_read_available(USART3, dst, capacity, received));
    app_device_leave(&ble, 1);
    return rc;
}
