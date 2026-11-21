#ifndef BSP_TIM_H
#define BSP_TIM_H
#include "bsp_types.h"

/* 用途：启动一个向上计数的 16 位定时器。参数：tim 为 TIM1~TIM10，psc 为预分频寄存器值，
 * arr 为自动重装值（1~65535）。注意：频率 = 定时器时钟 / (psc+1) / (arr+1)，同实例各通道共享。 */
bsp_status_t bsp_tim_init(TIM_TypeDef *tim, uint16_t psc, uint16_t arr);
/* 用途：按所需频率启动定时器。参数：tim 同上，frequency_hz 为目标更新/PWM 频率。
 * 注意：根据当前 APB 时钟选 16 位分频值，实际频率受整数分频约束。 */
bsp_status_t bsp_tim_init_hz(TIM_TypeDef *tim, uint32_t frequency_hz);
/* 用途：读取计数器。参数：tim 和可写结果指针。注意：计数到 arr 后回到 0。 */
bsp_status_t bsp_tim_counter(TIM_TypeDef *tim, uint16_t *value);
/* 用途：读出并清除更新标志。参数：tim 和溢出标志（1/0）地址。
 * 注意：轮询间发生多个周期溢出时，硬件只保留一个标志。 */
bsp_status_t bsp_tim_take_update(TIM_TypeDef *tim, uint8_t *updated);
/* 用途：在默认引脚启动一路高有效 PWM。参数：tim、通道号 1~4、占空比千分数 0~1000。
 * 注意：先初始化定时器；TIM6/7 仅有时基；引脚已分配时返回 BUSY。 */
bsp_status_t bsp_tim_pwm_init(TIM_TypeDef *tim, uint8_t channel, uint16_t duty_permille);
/* 在默认引脚按频率申请一路 PWM。空闲时自动启动时基，已用本接口启动的同频时基可共享。
 * 用 pwm_de_init 释放本通道，最后一个通道释放时自动关闭时钟。手工启动的时基请用 pwm_init。 */
bsp_status_t bsp_tim_pwm_init_hz(TIM_TypeDef *tim, uint8_t channel, uint32_t hz, uint16_t duty_permille);
/* 用途：在重映射引脚启动 PWM。参数：其余同 pwm_init，route 为手册表 3-4 的重映射编号。
 * TIM1 可选 0/1/3，TIM2 选 0~3，TIM3 选 0/2/3，TIM4/8 选 0/1，
 * TIM5 选 0，TIM9/10 选 0~3。注意：同一定时器各输出必须使用相同 route。 */
bsp_status_t bsp_tim_pwm_init_on(TIM_TypeDef *tim, uint8_t channel, uint8_t route, uint16_t duty_permille);
/* 用途：修改一个已开启通道的占空比。参数同 pwm_init。注意：0 全低，1000 全高。 */
bsp_status_t bsp_tim_pwm_write(TIM_TypeDef *tim, uint8_t channel, uint16_t duty_permille);
/* 立即写入通道的活动比较值。duty_permille 为 0~1000，仅作用于指定 PWM 通道。
 * 可在本个载波周期内截断脉冲或打开输出，不复位时基或改变其他通道。 */
bsp_status_t bsp_tim_pwm_write_immediate(TIM_TypeDef *tim, uint8_t channel, uint16_t duty_permille);
/* 为需要从载波起点产生的短脉冲保留 CNT。lock=1 要求没有邻道，保留期间其他通道申请返回 BUSY；
 * lock=0 解除保留，输出继续，调用者仍拥有本通道。 */
bsp_status_t bsp_tim_pwm_lock_phase(TIM_TypeDef *tim, uint8_t channel, uint8_t lock);
/* 清零 CNT 并立即设置占空比。仅当前通道处于活动状态时可执行，共享时返回 BUSY。 */
bsp_status_t bsp_tim_pwm_restart(TIM_TypeDef *tim, uint8_t channel, uint16_t duty_permille);
/* 同步更新独占 PWM 通道的频率和占空比，从新周期起点开始。frequency_hz 为目标 Hz，
 * duty_permille 为 0~1000；共享时同频请求只更新占空比，更改频率返回 BUSY。 */
bsp_status_t bsp_tim_pwm_set_frequency(TIM_TypeDef *tim, uint8_t channel,
                                       uint32_t frequency_hz, uint16_t duty_permille);
/* 用途：关闭一路 PWM。参数：tim 与通道号。注意：其他通道的相位、时基不变。 */
bsp_status_t bsp_tim_pwm_de_init(TIM_TypeDef *tim, uint8_t channel);
/* 用途：停止手工初始化的定时器并释放其通道。参数：tim 为 TIM1~TIM10。
 * 自动管理的 PWM 时基返回 BUSY，各使用者通过 pwm_de_init 释放自身通道。 */
bsp_status_t bsp_tim_de_init(TIM_TypeDef *tim);
/* 用途：在时基上开启一路输入捕获，记下有效边沿到来时的 CNT 值。
 * 参数：channel 为 1~4，route 与 PWM 相同，polarity 为 TIM_ICPolarity_Rising/Falling，
 * filter 为数字滤波编码 0~15。注意：本板 ECHO 接 PC11（TIM10 默认 CH4）。 */
bsp_status_t bsp_tim_capture_init(TIM_TypeDef *tim, uint8_t channel, uint8_t route,
                                  uint16_t polarity, uint8_t filter);
/* 用途：切换捕获边沿。参数：tim、通道、上升/下降沿。注意：会丢弃原有待处理的捕获。 */
bsp_status_t bsp_tim_capture_set_edge(TIM_TypeDef *tim, uint8_t channel, uint16_t polarity);
/* 用途：取最近的硬件捕获时间戳。参数：tim、通道、结果地址；单位为计数器 tick。
 * 注意：无新边沿返回 BUSY，漏读边沿返回 IO 并带回最新捕获值。捕获与 PWM 可共用时基。 */
bsp_status_t bsp_tim_capture_read(TIM_TypeDef *tim, uint8_t channel, uint16_t *tick);
/* 用途：关闭一路输入捕获。参数：tim 和通道。注意：其余通道继续工作。 */
bsp_status_t bsp_tim_capture_de_init(TIM_TypeDef *tim, uint8_t channel);
/* 用途：在单个输入脚上用硬件分别锁存上升沿和下降沿，测量正脉冲宽度。
 * 参数：tim、物理 channel 1~4、route、滤波编码 0~15。注意：先启动时基，在输入低电平时初始化。
 * 配对的比较通道（1↔2、3↔4）也被占用，其 GPIO 可另作他用。例如 PC11 用 TIM10 通道 4、route=0。 */
bsp_status_t bsp_tim_pulse_init(TIM_TypeDef *tim, uint8_t channel, uint8_t route, uint8_t filter);
/* 用途：一次取出脉宽。参数：tim、物理通道、计数 tick 数的接收地址。注意：脉宽须小于一个计数周期。
 * 未收齐两沿返回 BUSY，发生覆盖返回 IO；成功或覆盖后本次两沿已消费。 */
bsp_status_t bsp_tim_pulse_read(TIM_TypeDef *tim, uint8_t channel, uint16_t *width);
/* 丢弃该脉宽输入已锁存的边沿及溢出标记。channel 取申请时的物理通道，
 * 在前一脉冲结束、下一脉冲开始前调用，不释放引脚；输入仍为高电平时返回 BUSY。 */
bsp_status_t bsp_tim_pulse_rearm(TIM_TypeDef *tim, uint8_t channel);
/* 用途：释放该输入脚和两路捕获寄存器。参数：tim 和申请时的物理通道号。 */
bsp_status_t bsp_tim_pulse_de_init(TIM_TypeDef *tim, uint8_t channel);
/* 用途：启用 CH1/CH2 两相信号的四倍频正交计数。参数：tim、共同的 route、滤波编码 0~15。
 * 注意：独占定时器，无需先初始化时基。板载电机霍尔 PA6/PA7 可用 TIM3、route=0。 */
bsp_status_t bsp_tim_encoder_init(TIM_TypeDef *tim, uint8_t route, uint8_t filter);
/* 用途：获取自上次读取或初始化后的带符号增量。参数：tim、结果地址。
 * 注意：同一 APP 消费增量，每两次读取间的净位移绝对值须小于 32768 个计数。 */
bsp_status_t bsp_tim_encoder_delta(TIM_TypeDef *tim, int16_t *delta);
/* 用途：停止正交解码并释放 CH1/CH2。参数：tim 为已初始化的编码器实例。 */
bsp_status_t bsp_tim_encoder_de_init(TIM_TypeDef *tim);
#endif
