#ifndef __EVENT_H
#define __EVENT_H

#include <stdint.h>

#define KEY         0       /* 实体按键：code = key_id_t，value 不用 */
#define TOUCH        1       /* 触摸控件：code = 控件编号，value = 控件数值 */
#define QUIT        2       /* 让任务退出：只有 close() 会投，code/value 都是 0 */


typedef struct
{
    uint8_t type;    
    uint8_t code;       
    int32_t value;     
} message;

#endif
