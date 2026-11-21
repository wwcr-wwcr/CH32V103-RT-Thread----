/* APP_HAL 板级联动的默认配置。修改后重新编译，也可用工程的 -D 选项覆盖单项。
 * 时间单位 ms（1000 ms = 1 秒）。数值末尾的 U 表示无符号整数，f 表示单精度浮点数。
 */
#ifndef APP_HAL_CONFIG_H
#define APP_HAL_CONFIG_H

/* 调度与显示 */

#ifndef APP_HAL_SERVICE_MS
/** 应用线程两次调用 app_hal_update() 之间的建议延时（ms）。调小可更快响应按键和提醒。 */
#define APP_HAL_SERVICE_MS 10U
#endif

#ifndef APP_HAL_COUNTDOWN_DEFAULT_MS
/** 倒计时时长（ms）。app_hal_timer_countdown(0, 1) 使用此值，默认 60000 ms 即 1 分钟。
 * 传入非零时长可单独设置本次倒计时。有效范围为 1～5999000 ms（99 分 59 秒）。 */
#define APP_HAL_COUNTDOWN_DEFAULT_MS 60000U
#endif

#ifndef APP_HAL_METER_REFRESH_MS
/** 测量页面两次读取传感器并刷新温度、照度或声音读数的最短间隔（ms）。 */
#define APP_HAL_METER_REFRESH_MS 250U
#endif

#ifndef APP_HAL_CLOCK_REFRESH_MS
/** 时钟显示页两次读取 RTC 并更新时、分的最短间隔（ms）。RTC 自行走时。 */
#define APP_HAL_CLOCK_REFRESH_MS 250U
#endif

#ifndef APP_HAL_KEY_REPEAT_MS
/** 识别为长按后，继续按住加键或减键时每隔多少 ms 再调一步。值越小，连调越快。 */
#define APP_HAL_KEY_REPEAT_MS 120U
#endif

#ifndef APP_HAL_LAP_HOLD_MS
/** 秒表计圈后，本圈成绩在数码管上保留的时长（ms）。到时恢复显示总用时。 */
#define APP_HAL_LAP_HOLD_MS 2000U
#endif

/* 按键计数 */

#ifndef APP_HAL_COUNTER_DEFAULT_MIN
/** 默认计数的下限，也是启动和按键复位时显示的初值。四位数码管可显示的最小整数为 -999。 */
#define APP_HAL_COUNTER_DEFAULT_MIN 0
#endif

#ifndef APP_HAL_COUNTER_DEFAULT_MAX
/** 默认计数的上限。设为 9999 时，采用默认配置的计数器加到 9999 就停在该值。 */
#define APP_HAL_COUNTER_DEFAULT_MAX 9999
#endif

#ifndef APP_HAL_COUNTER_DEFAULT_STEP
/** 按键计数每一步增加或减少的数值，必须是正整数。例如设为 5，按一次加键就加 5。 */
#define APP_HAL_COUNTER_DEFAULT_STEP 1
#endif

#ifndef APP_HAL_COUNTER_DEFAULT_TARGET
/** 默认计数目标，须在上下限之间。加减时达到或跨过该值，会触发一次声光提示。 */
#define APP_HAL_COUNTER_DEFAULT_TARGET 100
#endif

#ifndef APP_HAL_COUNTER_NOTICE_MS
/** 计数达到目标后，整段声光提示持续的时间（ms）。 */
#define APP_HAL_COUNTER_NOTICE_MS 800U
#endif

/* 传感器采样与报警确认 */

#ifndef APP_HAL_SENSOR_PERIOD_MS
/** 阈值报警、风扇、测距、噪声和倾角模块检查传感器的最短间隔（ms）。 */
#define APP_HAL_SENSOR_PERIOD_MS 200U
#endif

#ifndef APP_HAL_SENSOR_MAX_GAP_MS
/** 两次有效采样仍可视为连续的最大间隔（ms）。超过时重新计算报警确认时长，噪声超限累计也跳过该间隔。 */
#define APP_HAL_SENSOR_MAX_GAP_MS 1000U
#endif

#ifndef APP_HAL_ALARM_CONFIRM_MS
/** 读数从首次达到触发阈值起，须连续越限多少 ms 才报警。默认 2 秒，设为 0 则首次达到即触发。
 * 已触发的报警在读数达到复位阈值时解除。 */
#define APP_HAL_ALARM_CONFIRM_MS 2000U
#endif

/* 共用数码管、RGB 和蜂鸣器的提示 */

#ifndef APP_HAL_ALERT_HZ
/** 声光提示期间蜂鸣器的音调频率（Hz）。设为 0 可静音。发声范围为 100～10000 Hz，数值越大音调越高。 */
#define APP_HAL_ALERT_HZ 2000U
#endif

#ifndef APP_HAL_ALERT_ON_MS
/** 重复声光提示中，每一次蜂鸣器发声和 RGB 亮灯的持续时间（ms）。两次提示之间有停顿时，此值须大于 0。 */
#define APP_HAL_ALERT_ON_MS 150U
#endif

#ifndef APP_HAL_ALERT_GAP_MS
/** 常规重复提示中，一次发声、亮灯结束到下一次开始之间的静音、灭灯时间（ms）。设为 0 则持续提示。 */
#define APP_HAL_ALERT_GAP_MS 350U
#endif

#ifndef APP_HAL_PRIORITY_NOTICE
/** 计数达标和距离提示的优先级。三个优先级均须大于 0，同时请求输出时数值较大者优先。 */
#define APP_HAL_PRIORITY_NOTICE 1U
#endif

#ifndef APP_HAL_PRIORITY_TIMER
/** 倒计时到时提醒的优先级。默认 2，可优先于普通提示使用数码管、RGB 和蜂鸣器。 */
#define APP_HAL_PRIORITY_TIMER 2U
#endif

#ifndef APP_HAL_PRIORITY_ALARM
/** 传感器阈值及倾角报警的优先级。默认 3，可在倒计时提醒期间优先显示、鸣响。 */
#define APP_HAL_PRIORITY_ALARM 3U
#endif

/* 温度、照度与声压阈值（以下按默认的高值报警说明） */

#ifndef APP_HAL_TEMP_TRIP_C
/** 高温触发温度（℃）。达到或超过此温度并持续满足确认时长时报警。 */
#define APP_HAL_TEMP_TRIP_C 40.0f
#endif

#ifndef APP_HAL_TEMP_RESET_C
/** 高温报警的解除温度（℃）。默认降到 38 ℃ 或以下时解除，与 40 ℃ 触发值留出温差以防反复报警。 */
#define APP_HAL_TEMP_RESET_C 38.0f
#endif

#ifndef APP_HAL_LIGHT_TRIP_LUX
/** 强光触发照度（lx，勒克斯）。达到或超过此值并持续满足确认时长时报警。 */
#define APP_HAL_LIGHT_TRIP_LUX 1000.0f
#endif

#ifndef APP_HAL_LIGHT_RESET_LUX
/** 强光报警的解除照度（lx）。照度降到此值或以下时解除。 */
#define APP_HAL_LIGHT_RESET_LUX 900.0f
#endif

#ifndef APP_HAL_NOISE_TRIP_DB
/** 噪声触发声压级（dB SPL，需先校准麦克风）。达到或超过此值时开始判断报警，也是噪声超限累计的默认门槛。 */
#define APP_HAL_NOISE_TRIP_DB 80.0f
#endif

#ifndef APP_HAL_NOISE_RESET_DB
/** 噪声报警的解除声压级（dB SPL）。声压级降到此值或以下时解除报警。 */
#define APP_HAL_NOISE_RESET_DB 75.0f
#endif

/* 超声波接近提示与经过计数 */

#ifndef APP_HAL_PROX_NEAR_MM
/** 近区边界（mm）。距离小于或等于此值时快速提示，首次进入近区计数加一。距离报警也以此值作默认触发阈值。 */
#define APP_HAL_PROX_NEAR_MM 150U
#endif

#ifndef APP_HAL_PROX_FAR_MM
/** 远区边界（mm），必须大于近区边界。距离达到或超过此值时停止接近提示，允许下次进入近区重新计数，并解除默认距离报警。 */
#define APP_HAL_PROX_FAR_MM 400U
#endif

#ifndef APP_HAL_PROX_GAP_NEAR_MS
/** 目标在近区时，两次声光提示之间停顿的时间（ms）。默认 100 ms，提示较密。 */
#define APP_HAL_PROX_GAP_NEAR_MS 100U
#endif

#ifndef APP_HAL_PROX_GAP_FAR_MS
/** 目标在近、远两个边界之间时，两次声光提示之间的停顿（ms）。默认 650 ms，提示较疏。 */
#define APP_HAL_PROX_GAP_FAR_MS 650U
#endif

/* 温控风扇 */

#ifndef APP_HAL_FAN_STOP_C
/** 风扇停转温度（℃）。温度降到此值或以下时停转。三个温度需满足 停转温度 < 启动温度 < 满速温度。 */
#define APP_HAL_FAN_STOP_C 28.0f
#endif

#ifndef APP_HAL_FAN_START_C
/** 风扇启动温度（℃）。达到此值时以最低运行占空比启动，在停转与启动温度之间保持此前的启停状态。 */
#define APP_HAL_FAN_START_C 30.0f
#endif

#ifndef APP_HAL_FAN_FULL_C
/** 风扇满速温度（℃）。从启动温度升至此值的过程中，占空比按温度线性增加，达到此值为 100%。 */
#define APP_HAL_FAN_FULL_C 42.0f
#endif

#ifndef APP_HAL_FAN_MIN_PERCENT
/** 风扇启动时的 PWM 占空比（1～100%）。例如 35 表示每个 PWM 周期有 35% 的时间驱动风扇。 */
#define APP_HAL_FAN_MIN_PERCENT 35U
#endif

#ifndef APP_HAL_FAN_FAULT_PERCENT
/** 温度读取失败或读数无效时使用的风扇占空比（0～100%），默认 100% 满速。传感器暂忙时保留原输出。 */
#define APP_HAL_FAN_FAULT_PERCENT 100U
#endif

/* 编码器测速与静态倾角 */

#ifndef APP_HAL_RPM_PERIOD_MS
/** 编码器累计脉冲后更新一次转速的最短间隔（ms）。间隔越长，低速时越容易积累足够的脉冲。 */
#define APP_HAL_RPM_PERIOD_MS 200U
#endif

#ifndef APP_HAL_TILT_TRIP_DEG
/** 倾角报警的触发角（度）。传感器 Z 轴朝上时约为 0°，达到此角并持续满足确认时长时报警。 */
#define APP_HAL_TILT_TRIP_DEG 30.0f
#endif

#ifndef APP_HAL_TILT_RESET_DEG
/** 倾角报警的解除角（度）。降到此值或以下时解除。两个角度需满足 0° ≤ 解除角 < 触发角 ≤ 180°。 */
#define APP_HAL_TILT_RESET_DEG 25.0f
#endif

#ifndef APP_HAL_TILT_G_TOLERANCE
/** 判断板子静止时，三轴合加速度相对 1 g 允许偏离多少 g。默认 0.15 表示接受 0.85～1.15 g。 */
#define APP_HAL_TILT_G_TOLERANCE 0.15f
#endif

#ifndef APP_HAL_TILT_STILL_DPS
/** 静态倾角测量允许的最大合角速度（度/秒）。三轴陀螺仪测得的合角速度超过此值时，跳过本次倾角判定。 */
#define APP_HAL_TILT_STILL_DPS 12.0f
#endif

/* 波形采集与串口发送 */

#ifndef APP_HAL_WAVE_VREF_MV
/** ADC 和 DAC 电压换算使用的默认参考电压（mV）。3300 表示 3.3 V，可填板上实测参考电压。 */
#define APP_HAL_WAVE_VREF_MV 3300U
#endif

#ifndef APP_HAL_WAVE_FRAME_SAMPLES
/** ADC 每帧采样点数（16～4096 点）。默认 512 点配合 16000 Hz，每帧覆盖约 32 ms。每点占 2 字节 RAM。 */
#define APP_HAL_WAVE_FRAME_SAMPLES 512U
#endif

#ifndef APP_HAL_WAVE_RATE_HZ
/** ADC 默认采样率（点/秒）。16000 表示每秒采 16000 点。范围为 2000～48000，采样时钟需能整分频。 */
#define APP_HAL_WAVE_RATE_HZ 16000U
#endif

#ifndef APP_HAL_WAVE_STREAM_ROWS_PER_UPDATE
/** 串口波形每次调用 app_hal_update() 最多发送的 CSV 行数（至少 1 行），含摘要和原始采样行。调小可减少单次发送耗时。 */
#define APP_HAL_WAVE_STREAM_ROWS_PER_UPDATE 4U
#endif

#ifndef APP_HAL_WAVE_STREAM_INTERVAL_MS
/** 串口波形发完一帧后，到开始采集下一帧的最短等待时间（ms）。流启动函数的间隔参数传 0 时使用此值。 */
#define APP_HAL_WAVE_STREAM_INTERVAL_MS 250U
#endif

/* DAC 波形表大小 */

#ifndef APP_HAL_WAVE_OUTPUT_SAMPLES
/** DAC 合成一个周期使用的采样点数，至少 8 点且为 4 的倍数。点数越多，正弦曲线越细，最高输出频率越低。 */
#define APP_HAL_WAVE_OUTPUT_SAMPLES 32U
#endif

#ifndef APP_HAL_WAVE_DAC_CAPACITY
/** DAC 波形表可存放的总点数，每点占 2 字节。指定周期输出需额外留 1 点回到偏置电压。
 * 默认 1024 点、每周期 32 点，一次最多完整输出 31 个周期。 */
#define APP_HAL_WAVE_DAC_CAPACITY 1024U
#endif

/* ADC 波形识别 */

#ifndef APP_HAL_WAVE_MIN_PP_CODES
/** 参与周期识别所需的最小 ADC 峰峰值（原始码值，1～4095）。一帧内最大值减最小值小于此门槛时跳过频率和波形判定。 */
#define APP_HAL_WAVE_MIN_PP_CODES 24U
#endif

#ifndef APP_HAL_WAVE_HYSTERESIS_PCT
/** 确认一次过中点的上升沿或下降沿时，信号须继续越过的幅度，占峰峰值的百分比（1～49%）。
 * 默认 5% 表示上升要到中点以上 5% 峰峰值，下降要到中点以下 5% 峰峰值，有助于过滤中点附近的抖动。 */
#define APP_HAL_WAVE_HYSTERESIS_PCT 5U
#endif

#ifndef APP_HAL_WAVE_SQUARE_RAIL_PCT
/** 方波高、低平台的幅度门槛（1～100%）。以峰峰值的一半为 100%，某点偏离中点达到设定比例时，就算作平台点。 */
#define APP_HAL_WAVE_SQUARE_RAIL_PCT 90U
#endif

#ifndef APP_HAL_WAVE_SQUARE_PLATEAU_PCT
/** 判定方波所需的平台点比例（1～100%）。默认 85 表示参与判别的采样点中，至少 85% 须落在高、低平台上。 */
#define APP_HAL_WAVE_SQUARE_PLATEAU_PCT 85U
#endif

#ifndef APP_HAL_WAVE_PERIOD_TOL_PCT
/** 判断周期是否稳定时，单周期相对平均周期允许的偏差（%）。另留 0.4 个采样点的余量，调小会更严格。 */
#define APP_HAL_WAVE_PERIOD_TOL_PCT 15U
#endif

#ifndef APP_HAL_WAVE_SHAPE_ERROR_PCT
/** 判别正弦波和三角波时，与理想波形允许的最大均方根误差（占半峰峰值的百分比）。调小会使判别更严格。 */
#define APP_HAL_WAVE_SHAPE_ERROR_PCT 10U
#endif

/* DAC 默认输出信号 */

#ifndef APP_HAL_WAVE_AMPLITUDE_MVPP
/** DAC 默认输出的峰峰值（mVpp），即最高电压减最低电压。1000 mVpp 表示在偏置电压上下各摆动 500 mV。 */
#define APP_HAL_WAVE_AMPLITUDE_MVPP 1000U
#endif

#ifndef APP_HAL_WAVE_FREQUENCY_HZ
/** DAC 默认输出频率（Hz），即每秒重复多少个完整周期。输出频率 × 每周期点数须 ≤ 100000 点/秒。 */
#define APP_HAL_WAVE_FREQUENCY_HZ 500U
#endif

#ifndef APP_HAL_WAVE_OFFSET_MV
/** DAC 波形的中心电压（mV），也是结束输出后保持的电压。默认 1650 mV 配合 1000 mVpp，输出范围为 1150～2150 mV。
 * 调整时，偏置减去或加上半个峰峰值均须落在 0～参考电压之间。 */
#define APP_HAL_WAVE_OFFSET_MV (APP_HAL_WAVE_VREF_MV / 2U)
#endif

#endif
