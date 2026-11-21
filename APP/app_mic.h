#ifndef APP_MIC_H
#define APP_MIC_H
#include "app_types.h"
#ifdef __cplusplus
extern "C" {
#endif

#define APP_MIC_FRAME_SAMPLES 128U
#define APP_MIC_MAX_SAMPLES   4096U
#define APP_MIC_MEASURE_RATE_HZ 48000U
#define APP_MIC_MEASURE_SAMPLES 1536U
/* 3.3 V 下的保守 SPL 有效码区间，可按本板实测的线性摆幅覆盖。
 * 依据数据手册表 4-46-1，4 kΩ 负载下 OPA 高端最多需留约 300 mV 裕量。 */
#ifndef APP_MIC_SPL_MIN_ADC
#define APP_MIC_SPL_MIN_ADC 16U
#endif
#ifndef APP_MIC_SPL_MAX_ADC
#define APP_MIC_SPL_MAX_ADC 3720U
#endif
#if APP_MIC_SPL_MIN_ADC >= APP_MIC_SPL_MAX_ADC || APP_MIC_SPL_MAX_ADC >= 4095U || APP_MIC_SPL_MIN_ADC < 1
#error Invalid microphone SPL input range
#endif
typedef struct {
    uint16_t min_adc, max_adc, peak_to_peak, clipped_samples;
    float mean_adc, rms_adc, dbfs;
} app_mic_level_t;

/* 启用板载 OPA3、ADC2，在 PA1（OPA3_OUT）采样。PC3 的 MIC_ADC 网络
 * 位于隔直电容 C31 后，原理图未标直流偏置；采 PA1 保留运放中点偏置。 */
app_status_t app_mic_init(void);
/* 读取一帧并更新各测量量，samples 可存放 count 个 12 位 ADC 原码。
 * count 范围 2~4096，采样为同步轮询，时间间隔受调度影响。 */
app_status_t app_mic_capture(uint16_t *samples, size_t count);
/* 采集默认帧并刷新缓存；适合只显示音量的应用。采样失败时缓存失效。 */
app_status_t app_mic_update(void);
/* 以下 getter 仅访问上次成功采集的同一帧，尚未采集返回 APP_ERR_STATE。
 * rms_adc 是去直流后的有效值，dBFS 以 2047.5 个 ADC 码为 0 dBFS，
 * 恒定输入为负无穷；clipped_samples 只统计 0 和 4095。绝对声压级需另行做声学校准。 */
app_status_t app_mic_get_level(app_mic_level_t *level);
app_status_t app_mic_get_mean(float *adc);
app_status_t app_mic_get_peak_to_peak(uint16_t *adc);
app_status_t app_mic_get_rms(float *adc);
app_status_t app_mic_get_dbfs(float *dbfs);
/* 现场采一帧并返回稳定单音频率（Hz）。面向 100~10000 Hz 的单音、蜂鸣器；
 * 过弱、无足够周期或各周期间隔明显不一致返回 APP_ERR_NOT_FOUND，保留原输出。
 * 以下四个测量/校准接口在线程中同步采样约 32 ms，短时占用 TIM4、TIM8 和 DMA1_CH7。
 * 所有测量成功采到帧后均刷新原有 level 缓存；宽带声音可能没有可报告的单一频率。 */
app_status_t app_mic_read_frequency_hz(float *frequency_hz);
/* 现场测数字电平（dBFS）。满量程正弦波约 -3.01 dBFS，恒定输入返回负无穷。 */
app_status_t app_mic_read_dbfs(float *dbfs);
/* 在已知 reference_db_spl 声压级的稳定参考声场中调用一次，保存本次会话的校准偏移。
 * 参考信号过弱或任一样本超出 SPL 有效码区间时返回 IO，保留已有校准。解初始化后校准失效。 */
app_status_t app_mic_calibrate_spl(float reference_db_spl);
/* 现场读出未计权的 32 ms RMS 声压级估计（dB SPL）。须先校准；沿用校准的信号强度
 * 与摆幅检查。有效码区间用于避免接近电源轨的 OPA 饱和；范围内的前级失真仍需实测。
 * 结果取决于板上模拟链路、频响和参考声源，适用于与校准一致的增益设置。 */
app_status_t app_mic_read_spl(float *db_spl);
/* 关闭麦克风的 ADC2、OPA3，清除缓存。 */
app_status_t app_mic_de_init(void);
#ifdef __cplusplus
}
#endif
#endif
