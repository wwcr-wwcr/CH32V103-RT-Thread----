#ifndef BSP_ADC_H
#define BSP_ADC_H
#include "bsp_types.h"

/* 用途：开启并校准指定 ADC。参数：adc 为 ADC1 或 ADC2。
 * 注意：实例独立，PCLK2/8 必须不超过 14 MHz。初始化无需提前选择引脚。 */
bsp_status_t bsp_adc_init(ADC_TypeDef *adc);
/* 用途：配置某一通道。参数：channel 为 0~15；ADC1 另支持温度 16、参考 17；
 * sample_time 使用 ADC_SampleTime_x。注意：内部通道使用 239.5 周期。 */
bsp_status_t bsp_adc_channel_init(ADC_TypeDef *adc, uint8_t channel, uint8_t sample_time);
/* 用途：连续读取 count 个校准后的 12 位码值。参数：adc、通道、可写数组、元素数。
 * 注意：先初始化实例。首次使用的通道自动按 239.5 周期初始化并保留至释放。
 * 同步轮询，采样间距受调度影响；超时时数组可能已写入前缀，解初始化后可重新校准。 */
bsp_status_t bsp_adc_read(ADC_TypeDef *adc, uint8_t channel, uint16_t *data, size_t count);
/* 定频读取 1~4096 个样本，rate_hz 为 2000~48000 Hz，时钟必须能精确整分频且转换时间足够短。
 * 本工程 data 须为片内 64 KiB SRAM 中的半字对齐数组。采集期间独占 TIM4、TIM8
 * 与 DMA1_CH7，均无需额外 GPIO。资源冲突返回 BUSY。限定触发次数并检查传输完整性；
 * 无逐样本时间戳。外部总线若阻塞读数到下一次转换完成，仅靠传输总数无法识别。
 * DMA 错误或超时时须丢弃整帧。
 * 成功后保留自动配置的 ADC 通道。失败时释放本次新申请的通道。 */
bsp_status_t bsp_adc_read_timed(ADC_TypeDef *adc, uint8_t channel, uint32_t rate_hz,
                                uint16_t *data, size_t count);
/* 用途：读取外接电容电极的原始 TouchKey 值。参数：adc、外部通道 0~15、结果地址。
 * 注意：先初始化实例。自动申请通道；其他模拟输入或输出占用同一电极时返回 BUSY。 */
bsp_status_t bsp_adc_touch_read(ADC_TypeDef *adc, uint8_t channel, uint16_t *value);
/* 用途：释放一个通道。参数：adc 与 channel 同上。注意：其他实例或通道继续工作。 */
bsp_status_t bsp_adc_channel_de_init(ADC_TypeDef *adc, uint8_t channel);
/* 用途：关闭实例及其全部通道。参数：adc 为 ADC1 或 ADC2。注意：采样期间返回 BUSY。 */
bsp_status_t bsp_adc_de_init(ADC_TypeDef *adc);
#endif
