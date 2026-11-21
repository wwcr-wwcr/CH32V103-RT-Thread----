#include "app_ethernet.h"
#include "app_internal.h"

/* 该板只使用 BSP 的板载 PHY 实例，BSP 已串行化整次操作。 */
app_status_t app_ethernet_init(const uint8_t mac[6])
{
    return app_from_bsp(bsp_eth_init(mac));
}
app_status_t app_ethernet_de_init(void)
{
    return app_from_bsp(bsp_eth_de_init());
}
app_status_t app_ethernet_link(uint8_t *connected)
{
    return app_from_bsp(bsp_eth_link(connected));
}
app_status_t app_ethernet_send(const uint8_t *frame, uint16_t length)
{
    return app_from_bsp(bsp_eth_send(frame, length));
}
app_status_t app_ethernet_receive(uint8_t *frame, uint16_t capacity, uint16_t *length)
{
    return app_from_bsp(bsp_eth_receive(frame, capacity, length));
}
