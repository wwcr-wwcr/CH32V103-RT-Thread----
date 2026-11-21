#include "app_usb_hub.h"
#include "app_internal.h"
#include "ch32v30x_usb.h"

#define HUB_ADDRESS 1U
#define HUB_NAK_TIMEOUT_MS 500U
#define HUB_TOKEN_POLLS 20000U

static app_device_t hub;
static uint8_t host_dma[512] __attribute__((aligned(4)));
static uint8_t address, ep0_size, configured, enumerated;
static uint8_t power_mode, power_good_units;

static uint16_t little16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[1] << 8 | p[0]);
}

static void delay_ms(unsigned ms)
{
    if (!ms) return;
    rt_tick_t start = rt_tick_get();
    rt_tick_t wait = rt_tick_from_millisecond(ms) + 1U;
    rt_tick_t elapsed = 0;
    do {
        rt_thread_delay(wait - elapsed);
        elapsed = rt_tick_get() - start;
    } while (elapsed < wait);
}

static bsp_status_t send_token(bsp_usb_token_t token, uint8_t data1, uint8_t *data,
                               uint16_t length, rt_tick_t start)
{
    bsp_usb_request_t request = {address, 0, data1, token, data, length};
    bsp_usb_reply_t reply;
    for (;;) {
        bsp_status_t status = bsp_usb_host_transfer(BSP_USB_HS, &request, &reply, HUB_TOKEN_POLLS);
        if (status != BSP_ERR_BUSY) {
            if (status == BSP_OK && token == BSP_USB_IN && reply.received != length)
                return BSP_ERR_IO;
            return status;
        }
        if ((rt_tick_t)(rt_tick_get() - start) >= rt_tick_from_millisecond(HUB_NAK_TIMEOUT_MS))
            return BSP_ERR_TIMEOUT;
        rt_thread_delay(1);
    }
}

/* 本驱动用到的控制请求只含 IN 数据或零数据。每笔传输的 DATA1/DATA0 独立起算。 */
static bsp_status_t control(uint8_t type, uint8_t request, uint16_t value, uint8_t index,
                            uint8_t *data, uint16_t length)
{
    uint8_t setup[8] = {type, request, (uint8_t)value, (uint8_t)(value >> 8),
                        index, 0, (uint8_t)length, (uint8_t)(length >> 8)};
    rt_tick_t start = rt_tick_get();
    bsp_status_t status = send_token(BSP_USB_SETUP, 0, setup, sizeof setup, start);
    uint8_t toggle = 1;
    while (status == BSP_OK && length) {
        uint16_t count = length < ep0_size ? length : ep0_size;
        status = send_token(BSP_USB_IN, toggle, data, count, start);
        toggle ^= 1U;
        data += count;
        length -= count;
    }
    if (status == BSP_OK) {
        bsp_usb_token_t ack = type & USB_REQ_TYP_IN ? BSP_USB_OUT : BSP_USB_IN;
        status = send_token(ack, 1, NULL, 0, start);
    }
    return status;
}

static bsp_status_t descriptor(uint8_t type, uint8_t *data, uint16_t length)
{
    uint8_t recipient = type == USB_DESCR_TYP_HUB ? HUB_GET_HUB_DESCRIPTOR : USB_REQ_TYP_IN;
    return control(recipient, USB_GET_DESCRIPTOR, (uint16_t)type << 8, 0, data, length);
}

/* 确认配置中有默认 alternate setting 的 Hub 接口和相应的中断 IN 端点。 */
static int has_hub_interface(const uint8_t *cfg, uint16_t length)
{
    int in_hub = 0, found = 0;
    for (uint16_t i = 0; i < length; ) {
        const uint8_t *entry = cfg + i;
        if (length - i < 2 || entry[0] < 2 || entry[0] > length - i) return 0;
        if (entry[1] == USB_DESCR_TYP_INTERF) {
            if (entry[0] < 9) return 0;
            in_hub = entry[3] == 0 && entry[4] && entry[5] == USB_DEV_CLASS_HUB;
        } else if (entry[1] == USB_DESCR_TYP_ENDP) {
            if (entry[0] < 7) return 0;
            if (in_hub && (entry[2] & 0x80U) && (entry[2] & 0x0FU) &&
                (entry[3] & 3U) == 3U && little16(entry + 4)) found = 1;
        }
        i += entry[0];
    }
    return found;
}

static bsp_status_t enumerate(void)
{
    bsp_usb_speed_t speed;
    bsp_status_t status = bsp_usb_host_reset(BSP_USB_HS, &speed);
    if (status != BSP_OK) return status;
    if (speed == BSP_USB_LOW_SPEED) return BSP_ERR_UNSUPPORTED;
    address = 0;
    ep0_size = 8;
    uint8_t buf[128];
    status = descriptor(USB_DESCR_TYP_DEVICE, buf, 8);
    if (status != BSP_OK) return status;
    if (buf[0] != 18 || buf[1] != USB_DESCR_TYP_DEVICE) return BSP_ERR_IO;
    if (buf[4] != USB_DEV_CLASS_HUB) return BSP_ERR_UNSUPPORTED;
    if ((buf[7] != 8 && buf[7] != 16 && buf[7] != 32 && buf[7] != 64) ||
        (speed == BSP_USB_HIGH_SPEED && buf[7] != 64)) return BSP_ERR_IO;
    ep0_size = buf[7];
    status = control(0, USB_SET_ADDRESS, HUB_ADDRESS, 0, NULL, 0);
    if (status != BSP_OK) return status;
    delay_ms(2);
    address = HUB_ADDRESS;
    status = descriptor(USB_DESCR_TYP_DEVICE, buf, 18);
    if (status != BSP_OK) return status;
    if (buf[0] != 18 || buf[1] != USB_DESCR_TYP_DEVICE || buf[4] != USB_DEV_CLASS_HUB ||
        buf[7] != ep0_size || !buf[17]) return BSP_ERR_IO;

    status = descriptor(USB_DESCR_TYP_CONFIG, buf, 9);
    if (status != BSP_OK) return status;
    uint16_t length = little16(buf + 2);
    if (buf[0] != 9 || buf[1] != USB_DESCR_TYP_CONFIG || length < 9 ||
        !buf[4] || !buf[5]) return BSP_ERR_IO;
    if (length > sizeof buf) return BSP_ERR_UNSUPPORTED;
    uint8_t config_value = buf[5];
    status = descriptor(USB_DESCR_TYP_CONFIG, buf, length);
    if (status != BSP_OK) return status;
    if (buf[0] != 9 || buf[1] != USB_DESCR_TYP_CONFIG || little16(buf + 2) != length ||
        buf[5] != config_value || !has_hub_interface(buf, length)) return BSP_ERR_IO;
    status = control(0, USB_SET_CONFIGURATION, config_value, 0, NULL, 0);
    if (status != BSP_OK) return status;
    configured = 1;
    status = descriptor(USB_DESCR_TYP_HUB, buf, 9);
    if (status != BSP_OK) return status;
    if (buf[0] != 9 || buf[1] != USB_DESCR_TYP_HUB) return BSP_ERR_IO;
    if (buf[2] != APP_USB_HUB_PORTS) return BSP_ERR_UNSUPPORTED;
    power_mode = buf[3] & 3U;
    power_good_units = buf[5];
    enumerated = 1;
    return BSP_OK;
}

/* 即使设备已经拔出，也尝试停止本地主机。锁竞争后保留释放资格，供 de_init 重试。 */
static bsp_status_t close_host(uint8_t *owned)
{
    bsp_status_t status = BSP_OK;
    enumerated = 0;
    if (configured) status = control(0, USB_SET_CONFIGURATION, 0, 0, NULL, 0);
    bsp_status_t release = bsp_usb_host_de_init(BSP_USB_HS);
    *owned = release == BSP_ERR_BUSY;
    if (!*owned || status == BSP_OK) configured = 0;
    return release != BSP_OK ? release : status;
}

app_status_t app_usb_hub_init(void)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t result = app_device_enter(&hub, 0);
    if (result != APP_OK) return result;
    uint8_t owned = 0;
    bsp_status_t status = bsp_usb_host_init(BSP_USB_HS, host_dma, sizeof host_dma);
    if (status == BSP_OK) {
        owned = 1;
        delay_ms(100); /* 上游连接去抖。 */
        status = enumerate();
        if (status != BSP_OK) {
            close_host(&owned);
            if (owned) status = BSP_ERR_BUSY;
        }
    }
    app_device_leave(&hub, owned);
    return app_from_bsp(status);
}

app_status_t app_usb_hub_read_port(uint8_t port, app_usb_hub_port_t *state)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    if (!port || port > APP_USB_HUB_PORTS || !state) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&hub, 1);
    if (result != APP_OK) return result;
    uint8_t data[4];
    bsp_status_t status = enumerated ? control(HUB_GET_PORT_STATUS, USB_GET_STATUS, 0, port,
                                               data, sizeof data) : BSP_ERR_STATE;
    if (status == BSP_OK) {
        uint16_t bits = little16(data);
        state->connected = (bits >> HUB_PORT_CONNECTION) & 1U;
        state->enabled = (bits >> HUB_PORT_ENABLE) & 1U;
        state->powered = (bits >> HUB_PORT_POWER) & 1U;
        state->over_current = (bits >> HUB_PORT_OVER_CURRENT) & 1U;
    }
    app_device_leave(&hub, 1);
    return app_from_bsp(status);
}

app_status_t app_usb_hub_set_port_power(uint8_t port, uint8_t on)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    if (!port || port > APP_USB_HUB_PORTS || on > 1) return APP_ERR_ARG;
    app_status_t result = app_device_enter(&hub, 1);
    if (result != APP_OK) return result;
    bsp_status_t status;
    if (!enumerated) status = BSP_ERR_STATE;
    else if (power_mode >= 2 && !on) status = BSP_ERR_UNSUPPORTED;
    else {
        status = control(HUB_SET_PORT_FEATURE, on ? USB_SET_FEATURE : USB_CLEAR_FEATURE,
                         HUB_PORT_POWER, port, NULL, 0);
        if (status == BSP_OK && on) delay_ms(2U * power_good_units);
    }
    app_device_leave(&hub, 1);
    return app_from_bsp(status);
}

app_status_t app_usb_hub_de_init(void)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t result = app_device_enter(&hub, 1);
    if (result != APP_OK) return result;
    uint8_t owned;
    bsp_status_t status = close_host(&owned);
    app_device_leave(&hub, owned);
    return app_from_bsp(status);
}
