#include "app_nfc.h"
#include "app_internal.h"
#include <string.h>

enum {
    FRAME_START = 0x55, CMD_ENABLE = 0x10, CMD_STATE = 0x11,
    CMD_SCAN = 0x12, CMD_READ = 0x13, CMD_WRITE = 0x14,
    MAX_PAYLOAD = APP_NFC_BLOCK_BYTES + 2U
};
static app_device_t device;
static uint8_t owns_spi, owns_cs;

static uint8_t checksum(const uint8_t *p, size_t length)
{
    uint8_t sum = 0;
    for (size_t i = 0; i < length; ++i) sum += p[i];
    return sum;
}

/* 校验接收长度后才访问校验和。读块错误应答可仅含状态（2 字节）。 */
static app_status_t exchange(uint8_t cmd, const uint8_t *payload, uint8_t n,
                             uint8_t *reply, uint8_t capacity, uint8_t *received)
{
    uint8_t tx[MAX_PAYLOAD + 4], rx[MAX_PAYLOAD + 4];
    if (!app_can_sleep()) return APP_ERR_STATE;
    if (!owns_cs || !owns_spi) return APP_ERR_STATE;
    tx[0] = FRAME_START;
    tx[1] = cmd;
    tx[2] = n;
    if (n) memcpy(tx + 3, payload, n);
    tx[3 + n] = checksum(tx + 1, n + 2U);
    app_status_t rc = app_from_bsp(bsp_gpio_write(BSP_PIN_NFC_NSS, 0));
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_spi_transfer(SPI2, tx, NULL, n + 4U));
    if (rc == APP_OK) {
        if (rt_thread_mdelay(cmd >= CMD_SCAN ? 100 : 10) != RT_EOK) rc = APP_ERR_IO;
        else rc = app_from_bsp(bsp_spi_transfer(SPI2, NULL, rx, capacity + 4U));
    }
    app_status_t release_rc = app_from_bsp(bsp_gpio_write(BSP_PIN_NFC_NSS, 1));
    if (rc != APP_OK) return rc;
    if (release_rc != APP_OK) return release_rc;
    uint8_t len = rx[2];
    if (rx[0] != FRAME_START || rx[1] != (uint8_t)(cmd | 0x80U) ||
        len > capacity || rx[3 + len] != checksum(rx + 1, len + 2U)) return APP_ERR_IO;
    memcpy(reply, rx + 3, len);
    *received = len;
    return APP_OK;
}

static app_status_t set_enabled(uint8_t enabled)
{
    uint8_t state, len;
    app_status_t rc = exchange(CMD_ENABLE, &enabled, 1, &state, 1, &len);
    if (rc == APP_OK && (len != 1 || state != enabled)) return APP_ERR_IO;
    return rc;
}

static app_status_t release_host(void)
{
    app_status_t rc = APP_OK;
    if (owns_spi) {
        rc = app_from_bsp(bsp_spi_de_init(SPI2));
        if (rc == APP_OK) owns_spi = 0;
    }
    if (owns_cs) {
        app_status_t cs_rc = app_from_bsp(bsp_gpio_de_init(BSP_PIN_NFC_NSS));
        if (cs_rc == APP_OK) owns_cs = 0;
        if (rc == APP_OK) rc = cs_rc;
    }
    return rc;
}

app_status_t app_nfc_init(void)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t rc = app_device_enter(&device, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_gpio_init(BSP_PIN_NFC_NSS, GPIO_Mode_Out_PP, 1));
    if (rc == APP_OK) {
        owns_cs = 1;
        rc = app_from_bsp(bsp_spi_init(SPI2, 0, 0, 3000000U, NULL));
    }
    if (rc == APP_OK) {
        owns_spi = 1;
        rc = set_enabled(1);
        if (rc != APP_OK) (void)set_enabled(0);
    }
    if (rc != APP_OK) (void)release_host();
    app_device_leave(&device, owns_cs || owns_spi);
    return rc;
}

app_status_t app_nfc_set_enabled(uint8_t enabled)
{
    if (enabled > 1U) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = set_enabled(enabled);
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_nfc_get_enabled(uint8_t *enabled)
{
    uint8_t state, len;
    if (!enabled) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = exchange(CMD_STATE, NULL, 0, &state, 1, &len);
    if (rc == APP_OK) {
        if (len != 1 || state > 1U) rc = APP_ERR_IO;
        else *enabled = state;
    }
    app_device_leave(&device, 1);
    return rc;
}

/* 套件协议返回 16 位小端状态，0 表示操作成功。 */
static app_status_t card_command(uint8_t cmd, const uint8_t *request,
                                 uint8_t request_len, uint8_t *data)
{
    uint8_t reply[MAX_PAYLOAD], received;
    uint8_t capacity = cmd == CMD_READ ? MAX_PAYLOAD : 2U;
    app_status_t rc = exchange(cmd, request, request_len, reply, capacity, &received);
    if (rc != APP_OK) return rc;
    if (received < 2) return APP_ERR_IO;
    uint16_t status = (uint16_t)reply[0] | ((uint16_t)reply[1] << 8);
    switch (status) {
    case 0: break;
    case 0x81: case 0x82: return APP_ERR_NOT_FOUND;
    case 0x83: return APP_ERR_STATE;
    case 0x85: return APP_ERR_ARG;
    default: return APP_ERR_IO;
    }
    if (received != capacity) return APP_ERR_IO;
    if (data) memcpy(data, reply + 2, APP_NFC_BLOCK_BYTES);
    return APP_OK;
}

static app_status_t scan(void)
{
    const uint8_t once = 1;
    return card_command(CMD_SCAN, &once, 1, NULL);
}

app_status_t app_nfc_scan(void)
{
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = scan();
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_nfc_read_block(uint8_t block, uint8_t data[APP_NFC_BLOCK_BYTES])
{
    if (!data || block >= 64U) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = scan();
    if (rc == APP_OK) rc = card_command(CMD_READ, &block, 1, data);
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_nfc_write_data_block(uint8_t block, const uint8_t data[APP_NFC_BLOCK_BYTES])
{
    uint8_t request[1U + APP_NFC_BLOCK_BYTES];
    if (!data || !block || block >= 64U || block % 4U == 3U) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = scan();
    if (rc == APP_OK) {
        request[0] = block;
        memcpy(request + 1, data, APP_NFC_BLOCK_BYTES);
        rc = card_command(CMD_WRITE, request, sizeof(request), NULL);
    }
    app_device_leave(&device, 1);
    return rc;
}

app_status_t app_nfc_de_init(void)
{
    if (!app_can_sleep()) return APP_ERR_STATE;
    app_status_t rc = app_device_enter(&device, 1);
    if (rc != APP_OK) return rc;
    rc = set_enabled(0);
    app_status_t release_rc = release_host();
    app_device_leave(&device, owns_cs || owns_spi);
    return rc != APP_OK ? rc : release_rc;
}
