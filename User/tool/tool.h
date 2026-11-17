/******************************************************************************
 * 文件名   : tool.h
 * 说明     : 工具箱总入口 —— 一行 include 全带走
 *
 *     #include "tool/tool.h"
 *
 * 里面全是 static inline（或纯宏），没有 .c 文件，所以：
 *     · 不用往 MounRiver 工程里加源文件、不用动 .cproject；
 *     · 不用改 obj 目录下的 subdir.mk，按 F5 直接编；
 *     · 没用到的函数会被编译器丢掉，不占 Flash。
 *
 * 基础模块：
 *     tool_math.h     夹取 / 线性映射 / 整数开方 / log2 / 伪随机
 *     tool_filter.h   EMA / 滑动平均 / 中值 / 迟滞 / 死区
 *     tool_timer.h    非阻塞软件定时器（毫秒级，抗回绕）
 *     tool_fmt.h      数字/时间转字符串（不依赖 printf）
 *     tool_bit.h      位段读写 / 位反转 / popcount
 *     tool_crc.h      CRC8 / CRC16-MODBUS / 累加和
 *     tool_plot.h     曲线自动量程（配 ui/common.h 的 ui_plot_t）
 *
 * 面向赛题补的模块（详见 tools/toolkit.html）：
 *     tool_wave.h     正弦表 / 相位累加 / PWM 占空比 / 步进间隔   -> 任务 6、4
 *     tool_color.h    HSV / 渐变 / RGB565                        -> 任务 2、UI
 *     tool_seq.h      非阻塞序列（闪 N 次、鸣 N 声）              -> 任务 2、8
 *     tool_fixed.h    Q15/Q8、四舍五入、金额、BMI                 -> 任务 7
 *     tool_draw.h     直线 / 圆 / 圆弧 / 指针（逐点回调）          -> 任务 3、4、6
 *
 * 依赖：只有 <stdint.h>（tool_draw 另外引用 tool_wave 的查表）。
 *       不 include rtthread.h，也不碰任何 HAL，所以任务线程、UI 线程、
 *       甚至中断里都能调；带 static 状态的几个结构体别跨线程共用。
 ******************************************************************************/

#ifndef __TOOL_H
#define __TOOL_H

#include "tool_math.h"
#include "tool_filter.h"
#include "tool_timer.h"
#include "tool_fmt.h"
#include "tool_bit.h"
#include "tool_crc.h"
#include "tool_plot.h"

#include "tool_wave.h"
#include "tool_color.h"
#include "tool_seq.h"
#include "tool_fixed.h"
#include "tool_draw.h"

#endif
