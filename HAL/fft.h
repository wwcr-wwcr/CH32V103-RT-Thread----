/******************************************************************************
 * 文件名   : fft.h
 * 说明     : 幅值谱计算 —— 输入一段采样，输出每个频点的幅值强度
 *
 * 特点：
 *     1. 两个数组就够：in 是采样、out 是幅值，不需要额外的中间缓冲
 *        （内部用 Goertzel 算法逐频点计算，只占几个局部变量，不占堆/静态 RAM）
 *     2. 自动识别输入数组的长度：用 FFT_RUN(in, out) 宏
 *     3. 不依赖 libm：余弦用递推、开方用牛顿迭代，全在文件内实现
 *
 * 长度约定：
 *     - in[]  必须是**真正的数组**（不能是传进来的指针），否则 FFT_RUN 取不到长度
 *     - out[] 需要 count/2 + 1 个元素（bin0 = 直流，bin1.. 依次 1 倍频、2 倍频…）
 *     - count 建议 64..512；频率分辨率 = 采样率 / count
 *
 * 幅值标定：
 *     输入幅值为 A 的正弦（比如 2048±800），在主频那根柱子上的输出约等于 A（800）。
 *     所以 out[k] 可以直接和时域码值比较，也能直接换算成毫伏。
 ******************************************************************************/

#ifndef __HAL_FFT_H
#define __HAL_FFT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FFT_COUNT_MIN   16U      /* 少于这个点数没有意义 */
#define FFT_COUNT_MAX   1024U    /* 上限 1024；N=1024 约 25 ms（O(n^2)），
                                  /* 只看某频段的话改 wave_ana.h 里的 WAVE_ANA_BAND_LO/HI */

/**
 * @brief  算一遍幅值谱
 * @param  in   采样数组（12 位码值，0~4095），长度 count
 * @param  out  幅值数组，长度 count/2 + 1；out[0] = 直流分量的绝对值
 * @param  count 采样点数 16..512 
 * @return 实际计算的点数（= count）；参数非法返回 0
 * @note   in 里会先减掉平均值再算谱，所以 out[0] 之外不会有直流造成的假峰
 */
 //k = (f_target × count) / rate_hz
uint16_t fft_process(const uint16_t *in, uint16_t *out, uint16_t count);

/**
 * @brief  自动取数组长度的版本
 * @note   只能传真正的数组变量：
 *             uint16_t samples[256];  uint16_t amp[129];
 *             FFT_RUN(samples, amp);
 *         传指针（uint16_t *p）编译会算错长度。
 */
#define FFT_RUN(in, out)   fft_process((in), (out), (uint16_t)(sizeof(in) / sizeof((in)[0])))       //out最大值2048

#ifdef __cplusplus
}
#endif

#endif
