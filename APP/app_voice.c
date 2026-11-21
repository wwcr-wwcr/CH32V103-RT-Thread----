#include "app_voice.h"
#include "app_internal.h"
#include <string.h>

static app_device_t voice;
static uint8_t ring[129], frame[5], used;
app_status_t app_voice_init(void)
{
    app_status_t rc = app_device_enter(&voice, 0);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_init(UART6, 9600, 0));
    if (rc == APP_OK) {
        rc = app_from_bsp(bsp_uart_buffer_rx(UART6, ring, sizeof ring));
        if (rc != APP_OK) bsp_uart_de_init(UART6);
    }
    used = 0;
    app_device_leave(&voice, rc == APP_OK);
    return rc;
}
app_status_t app_voice_de_init(void)
{
    app_status_t rc = app_device_enter(&voice, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_de_init(UART6));
    if (rc == APP_OK) used = 0;
    app_device_leave(&voice, rc != APP_OK);
    return rc;
}
app_status_t app_voice_send(uint8_t target, uint8_t action)
{
    const uint8_t packet[] = {0xa5, target, action, 0x0d, 0x0a};
    app_status_t rc = app_device_enter(&voice, 1);
    if (rc != APP_OK) return rc;
    rc = app_from_bsp(bsp_uart_write(UART6, packet, sizeof packet, NULL));
    app_device_leave(&voice, 1);
    return rc;
}
app_status_t app_voice_receive(app_voice_command_t *command)
{
    if (!command) return APP_ERR_ARG;
    app_status_t rc = app_device_enter(&voice, 1);
    if (rc != APP_OK) return rc;
    rc = APP_ERR_BUSY;
    for (size_t i = 0; i < sizeof ring; ++i) {
        uint8_t byte;
        size_t count;
        rc = app_from_bsp(bsp_uart_read_available(UART6, &byte, 1, &count));
        if (rc != APP_OK) {
            if (rc == APP_ERR_IO) used = 0;
            break;
        }
        rc = APP_ERR_BUSY;
        if (!used && byte != 0x5a) continue;
        frame[used++] = byte;
        if (used < sizeof frame) continue;
        if (frame[3] == 0x0d && frame[4] == 0x0a) {
            command->target = frame[1];
            command->action = frame[2];
            used = 0;
            rc = APP_OK;
            break;
        }
        /* 坏帧中的下一个帧头可能已经到达，保留它及其后的字节。 */
        uint8_t offset = 1;
        while (offset < used && frame[offset] != 0x5a) ++offset;
        used -= offset;
        memmove(frame, frame + offset, used);
    }
    app_device_leave(&voice, 1);
    return rc;
}
