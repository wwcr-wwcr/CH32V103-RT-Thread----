#ifndef APP_TYPES_H
#define APP_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* 所有板载设备共用的结果。BUSY 也可表示采集尚未完成，详见对应接口。 */
typedef enum {
    APP_OK = 0, APP_ERR_ARG = -1, APP_ERR_BUSY = -2, APP_ERR_STATE = -3,
    APP_ERR_TIMEOUT = -4, APP_ERR_IO = -5, APP_ERR_UNSUPPORTED = -6,
    APP_ERR_NOT_FOUND = -7
} app_status_t;
#endif
