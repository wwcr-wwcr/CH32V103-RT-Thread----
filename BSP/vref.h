/******************************************************************************
 * 文件名   : vref.h
 * 说明     : 板级电压标定 —— 码值和毫伏之间只从这里换算
 *
 * 为什么需要它：
 *     这块板实测 VDDA ≈ 3.6V（DAC 码值 2048 用万用表量到 1.80V），
 *     不是标称的 3.3V。以前各文件里写死 3300/4095，导致
 *         DAC 输出偏高约 +9%
 *         ADC 读数偏低约 -8.5%
 *     而且回环自测（DAC->ADC）两个误差互相抵消，看着"完美"，其实绝对值是错的。
 *
 * 约定：
 *     1. 业务代码里不要再出现 3300 / 4095 这种换算，统一调本模块。
 *     2. 默认值按实测填 3600；换板子用 `vdda <mV>` 命令现测现改。
 *     3. 只做线性换算，不补偿零点偏移（实测 0V 输出时约 7mV，可忽略）。
 ******************************************************************************/

#ifndef __BOARD_VREF_H
#define __BOARD_VREF_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 默认 VDDA（mV）：实测值，不是 3300
 *  标定依据（2026-11-25，万用表量 PA5）：
 *      码值 1877  ->  1.680 V
 *      满量程 4095 -> 1680 * 4095 / 1877 ≈ 3666 mV
 *  换板子重新量一次，用 `vdda <mV>` 命令现改。
 */
#define VREF_VDDA_MV_DEFAULT   3666U

/** 允许的运行期标定范围（超出按边界夹住） */
#define VREF_VDDA_MV_MIN       3000U
#define VREF_VDDA_MV_MAX       3800U

/** 当前使用的 VDDA（mV） */
uint16_t vref_vdda_mv(void);

/** 运行期标定；传 0 恢复默认值 */
void vref_set_vdda_mv(uint16_t mv);

/** 12 位码值 -> 毫伏（四舍五入） */
uint16_t vref_code_to_mv(uint32_t code);

/** 毫伏 -> 12 位码值（四舍五入，超过 VDDA 钳到满量程 4095） */
uint16_t vref_mv_to_code(uint32_t mv);

#ifdef __cplusplus
}
#endif

#endif
