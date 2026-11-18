#ifndef __EVENT_H
#define __EVENT_H

#include <stdint.h>

typedef struct
{
    uint8_t type;       /* 0 = 按键，1 = 控件事件 */
    uint8_t code;       /* type=0：key_id_t；type=1：控件编号 */
    int32_t value;      /* type=1：控件当前值 */
} message;

#endif
