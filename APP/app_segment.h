#ifndef APP_SEGMENT_H
#define APP_SEGMENT_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 初始化 CH422G 与四位数码管，清屏并启动芯片内自动扫描。冷启动时等待芯片上电复位，
 * 若剩余等待期间无法挂起当前线程则返回 BUSY。 */
app_status_t app_segment_init(void);
/* 关闭段输出和动态扫描，释放两根控制线。 */
app_status_t app_segment_de_init(void);
/* 显示整数。value 取 -999~9999，自动隐藏前导零。 */
app_status_t app_segment_show_number(int32_t value);
/* 显示带小数点的定点数。例如 (-37,1) 显示 -3.7。decimals 取 0~3，
 * 包含负号和小数点前的 0 在内，数字和负号总数须不超过四位。 */
app_status_t app_segment_show_fixed(int32_t value, uint8_t decimals);
/* 按文本从左到右显示，保留前导零，空串清屏。例如 "3337"、"-1.2.3"。
 * text 为以 '\0' 结尾的字符串。数字、减号、空格各占一位；小数点点亮前一位的点段。
 * 开头的点或连续的额外点各占一位。超过四位、含其他字符或空指针返回 APP_ERR_ARG，
 * 参数错误时保留原显示。 */
app_status_t app_segment_show_text(const char *text);
/* 熄灭所有数字和小数点，保留设备。 */
app_status_t app_segment_clear(void);
#ifdef __cplusplus
}
#endif
#endif
