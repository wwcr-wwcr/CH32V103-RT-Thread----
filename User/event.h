/******************************************************************************
 * 文件名   : event.h
 * 说明     : 全系统唯一的事件结构 —— 模块之间唯一的"协议"，没有函数
 *
 * 谁生产：
 *     key 线程          type=0, code=KEY_ID_x                      按键按下
 *     触摸按钮回调      type=0, code=KEY_ID_x                      合成按键（和实体键同一条路）
 *     滑条 / 触摸按钮   type=1, code=控件编号, value=控件当前值      控件事件
 *
 * 谁消费：
 *     kernel 从队列里取出来 -> 交给注册的事件处理函数（main 里绑的 app_handle）
 *     app 再按 type 分：0 找当前任务的 on_key，1 找当前任务的 on_set
 *     任务线程内部再用 type=2 表示"退出线程"（app.c 自己发的，不对外）
 *
 * type 不额外定义宏，直接写数字，含义就上面这张表：
 *     0 = 按键
 *     1 = 控件事件
 ******************************************************************************/

#ifndef __EVENT_H
#define __EVENT_H

#include <stdint.h>

typedef struct
{
    uint8_t type;       /* 0 = 按键，1 = 控件事件 */
    uint8_t code;       /* type=0：key_id_t；type=1：控件编号 */
    int32_t value;      /* type=1：控件当前值 */
} evt_t;

#endif
