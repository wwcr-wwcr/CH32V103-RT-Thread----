# HSM 串口命令

在板子的 RT-Thread `msh >` 提示符输入 `hsm 命令 参数...` 后按回车。使用原有 uart1 控制台，115200、8N1。输入 `hsm help` 查看命令，`hsm help adc` 按前缀筛选，`hsm status` 查看工作线程是否已创建和最近一次后台更新的状态码。

## 操作示例

默认倒计时到时声光报警。逐行发送，等待每条命令的回执。

```text
hsm timer.init
hsm timer.down 0 1
hsm timer.get
```

到时可按 K4 或输入 `hsm timer.ack` 确认。结束后输入 `hsm timer.deinit`。还需释放整个调度器时，先释放所有已开启模块，再执行 `hsm core.deinit`。

显示温度并配置高温报警，达到 40 ℃ 持续 2 秒触发，回落到 38 ℃ 解除。

```text
hsm meter.init
hsm meter.page 0
hsm alarm.init
hsm alarm.set 0 40 38 2000
hsm alarm.get 0
```

DAC2 输出 500 Hz 正弦波，峰峰值 1000 mV，偏置 1650 mV。

```text
hsm dac.init 2 3300
hsm dac.start 1 500 1000 1650
hsm dac.stop
hsm dac.deinit
```

## 参数和回执

- 布尔参数用 0 或 1。实数使用十进制，例如 `-1.25`。参数之间用空格，每次发送一行。
- 测量页编号依次为 0 温度、1 照度、2 dBFS、3 dB SPL、4 频率。视图为 0 实时、1 最小、2 最大。声压先在已知参考声场使用 `meter.cal` 或 `mic.cal` 校准。
- 报警来源为 0 温度、1 照度、2 声压、3 距离。波形类型为 1 正弦、2 三角、3 方波，测量结果中的 0 表示未知。
- 编辑定点参数时数值按最小显示单位传入，例如小数位为 1 时，初值 235 显示为 23.5。计数方向取 +1 或 -1。
- ADC 号取 1 或 2，通道取 0～15。DAC 通道取 1 或 2。使用回环前将对应的 DAC 输出接到可用 ADC 输入。默认参数和板级资源限制见[使用说明](使用说明.md)。
- 查询命令先输出带字段名的数据，末尾为 `OK`。失败返回 `ERR` 和状态码：-1 参数错误，-2 忙，-3 状态或线程不对，-4 超时，-5 I/O 失败，-6 不支持，-7 未找到有效结果。
- 各模块先 init 后操作，最后 deinit。所有串口操作和周期更新由同一个工作线程执行，启动计时、流输出后可继续输入命令。其他应用线程已经持有 HAL 时，需由原线程释放后才能从串口接管。
- `adc.trigger` 打印触发后采到的点并留存该批数据。`adc.analyze` 使用这批数据分析，传入采集时的采样率与参考电压。流模式下 `adc.stream` 自动分批打印 CSV，`adc.stop` 停止。

## 命令速查

每行命令均需加前缀 `hsm `。相同模块的命令放在一起。普通运行只需初始化后启动所需功能，工作线程会自动执行模块的 update/poll。

| 命令 | 按顺序输入的参数 |
|---|---|
| `adc.analyze` | 采样率 参考mV |
| `adc.deinit` | 无 |
| `adc.init` | ADC号 通道 采样率 参考mV |
| `adc.measure` | 无 |
| `adc.pulse` | 无 |
| `adc.stop` | 无 |
| `adc.stream` | 间隔ms 原始数据 |
| `adc.trigger` | 阈值mV 上升沿 超时ms 点数 |
| `alarm.ack` | 来源 |
| `alarm.default` | 来源 |
| `alarm.deinit` | 无 |
| `alarm.get` | 来源 |
| `alarm.init` | 无 |
| `alarm.off` | 来源 |
| `alarm.set` | 来源 触发值 复位值 确认毫秒 |
| `clock.deinit` | 无 |
| `clock.edit` | 无 |
| `clock.finish` | 保存 |
| `clock.init` | 无 |
| `clock.show` | 无 |
| `core.deinit` | 无 |
| `core.init` | 无 |
| `counter.config` | 最小 最大 初值 步长 目标 循环 达标提示 |
| `counter.deinit` | 无 |
| `counter.get` | 无 |
| `counter.init` | 无 |
| `counter.start` | 无 |
| `counter.step` | 方向 |
| `counter.target` | 目标 |
| `dac.burst` | 波形 频率Hz 峰峰值mV 偏置mV 周期数 |
| `dac.deinit` | 无 |
| `dac.get` | 无 |
| `dac.init` | 通道 参考mV |
| `dac.start` | 波形 频率Hz 峰峰值mV 偏置mV |
| `dac.stop` | 无 |
| `fan.deinit` | 无 |
| `fan.get` | 无 |
| `fan.init` | 无 |
| `fan.set` | 停止温度 启动温度 满速温度 最低占空比 |
| `loop.deinit` | 无 |
| `loop.init` | DAC通道 ADC号 ADC通道 采样率 参考mV |
| `loop.test` | 波形 频率Hz 峰峰值mV 偏置mV |
| `meter.cal` | 参考声压dB |
| `meter.deinit` | 无 |
| `meter.get` | 页 |
| `meter.hold` | 冻结 |
| `meter.init` | 无 |
| `meter.page` | 页 |
| `meter.reset` | 无 |
| `meter.view` | 视图 |
| `mic.cal` | 参考声压dB |
| `noise.deinit` | 无 |
| `noise.get` | 无 |
| `noise.init` | 无 |
| `noise.reset` | 无 |
| `noise.set` | 阈值dBSPL |
| `param.deinit` | 无 |
| `param.edit` | 初值 最小 最大 步长 小数位 |
| `param.finish` | 保存 |
| `param.get` | 无 |
| `param.init` | 无 |
| `proximity.deinit` | 无 |
| `proximity.get` | 无 |
| `proximity.init` | 无 |
| `proximity.reset` | 无 |
| `proximity.set` | 近界mm 远界mm |
| `proximity.show` | 显示经过次数 |
| `rpm.deinit` | 无 |
| `rpm.get` | 无 |
| `rpm.init` | 每圈计数 |
| `rpm.show` | 无 |
| `tilt.deinit` | 无 |
| `tilt.get` | 无 |
| `tilt.init` | 无 |
| `tilt.set` | 触发角 复位角 确认毫秒 |
| `tilt.show` | 无 |
| `timer.ack` | 无 |
| `timer.deinit` | 无 |
| `timer.down` | 毫秒 到时报警 |
| `timer.get` | 无 |
| `timer.init` | 无 |
| `timer.lap` | 无 |
| `timer.pause` | 无 |
| `timer.resume` | 无 |
| `timer.stop` | 无 |
| `timer.up` | 无 |
