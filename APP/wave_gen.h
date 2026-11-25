/******************************************************************************
 * 文件名   : wave_gen.h
 * 说明     : 波形发生器（PA5 / DAC2）—— 内置 256 点高精度正弦码表，4 种波形
 *
 * 分工：
 *     HAL/siggen   只负责"把表按速率打出去"
 *     本文件       负责算波形：正弦 / 三角 / 方波 / 调频扫频
 *
 * ★ 所有电压参数都用「毫伏(mV)」，不用自己算码值：
 *     想输出 0.8V~2.5V 就写 800, 2500
 *     最大可到板子的 VDDA（实测约 3666mV），换算见 BSP/vref.h
 *     1 mV 约等于 1.12 个码值（VDDA=3.666V），分辨率约 0.9 mV，够用
 *     注意：VDDA 不是标称 3.3V，别再用 3300 近似，统一走 vref_mv_to_code()
 *
 * 参数说明：
 *     freq_hz   输出频率（Hz），1..20000
 *     low_mv    输出最低电压（mV，0..3299）
 *     high_mv   输出最高电压（mV，low_mv+1..3300）
 *     peak_pct  三角波尖峰位置 5..95（50 = 对称；<50 尖峰偏左，>50 偏右）
 *     duty_pct  方波占空比 5..95（高电平占比，百分比）
 *     sweep_ms  一次扫频的时长（毫秒），100..10000，输出循环重复
 ******************************************************************************/

#ifndef __WAVE_GEN_H
#define __WAVE_GEN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WAVE_GEN_FREQ_MAX     20000U    /* 输出频率上限 */
/* 名义参考电压：只用于文档/兼容旧代码，真正换算走 BSP/vref（实测 3.6V） */
#define WAVE_GEN_VREF_MV      3300U
#define WAVE_GEN_SWEEP_MS_MIN 100U
#define WAVE_GEN_SWEEP_MS_MAX 10000U

/**
 * @brief  输出正弦波
 * @param  freq_hz  频率 1..20000
 * @param  low_mv   输出最低电压（mV，0..3299）
 * @param  high_mv  输出最高电压（mV，low_mv+1..3300）
 * @return 0 = 成功，负 = 参数错
 * @note   用内置 256 点码表；频率高时自动降到每周期 32 点，保证采样率不超限
 */
int wave_gen_sine(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv);

/**
 * @brief  输出三角波
 * @param  freq_hz   频率 1..20000
 * @param  low_mv    最低电压（mV）
 * @param  high_mv   最高电压（mV）
 * @param  peak_pct  尖峰位置 5..95：50 = 等腰；30 = 尖峰偏左（升得快、落得慢）
 * @return 0 = 成功，负 = 参数错
 */
int wave_gen_tri(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv, uint8_t peak_pct);

/**
 * @brief  输出方波
 * @param  freq_hz   频率 1..20000
 * @param  low_mv    低电平电压（mV）
 * @param  high_mv   高电平电压（mV）
 * @param  duty_pct  占空比 5..95（高电平占比，50 = 对称方波）
 * @return 0 = 成功，负 = 参数错
 */
int wave_gen_square(uint32_t freq_hz, uint16_t low_mv, uint16_t high_mv, uint8_t duty_pct);

/**
 * @brief  调频输出（线性扫频）：从 f_start 扫到 f_end，一次用 sweep_ms，循环重复
 * @param  f_start   起始频率 1..20000
 * @param  f_end     结束频率 1..20000（超出采样能力时自动截断）
 * @param  sweep_ms  一次扫频时长 100..10000
 * @param  low_mv    最低电压（mV）
 * @param  high_mv   最高电压（mV）
 * @return 0 = 成功，负 = 参数错
 * @note   1024 点缓冲下 fs = 1024/时长，结束频率上限 ≈ fs/3：
 *         200ms→约1700Hz、500ms→约680Hz、1000ms→约340Hz
 */
int wave_gen_sweep(uint32_t f_start, uint32_t f_end, uint32_t sweep_ms,
                   uint16_t low_mv, uint16_t high_mv);

/**
 * @brief  停止输出，PA5 归零
 * @note   退出页面前必须调，否则回到菜单后还在输出
 */
int wave_gen_stop(void);

#ifdef __cplusplus
}
#endif

#endif
