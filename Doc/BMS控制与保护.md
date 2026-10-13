# BMS 控制与保护

实现位置：`Middleware/bms_ctrl`。依据所提供的 20260929 V00 原理图、`bsp_gpio.h` 和芯片资源说明；模块尚未接入应用入口和周期调度，也未做硬件测试。

## 电路与接口

| 信号 | MCU 引脚 | 电路作用 / 模块行为 |
| --- | --- | --- |
| KEY | PF3 | USB 板 Q4 为 NPN，MCU 高电平将 IP5326 KEY 拉低；默认释放 |
| VIN_EN | PB5 | Q3 拉低 Q1 PMOS 栅极，高有效；保留 USB 输入通路 |
| VBAT_EN | PB4 | Q5 拉低 Q2 PMOS 栅极，高有效；控制电池到 IP5326 BAT 的通路 |
| CTL | PA2 | 直接接 USB 板 IP3108A CTL；有效电平未提供，模块不改写此引脚 |
| USB_IN | PA10 | 来自 VIN-USB 的 100K/200K 分压；连续稳定 20ms 后更新插入状态 |
| LED1 / LED3 | PF0 / PF1 | 返回原始电平；不能按四灯模式表推算准确电量或识别硬件故障 |

PB5/PB4 的定义已按主控板原理图确认。PB6 原 BMS_VIN_EN 定义已移除。其他已有代码仍有与原理图不一致的映射，例如 PWM_WH、显示 DIO 和按键；本模块不更改这些驱动的映射。

IP3108A 的 NTC1、分流电阻 R18 及 CO/DO 连接构成硬件电池保护。MCU 没有独立的 CO/DO 故障反馈，软件不能声明已经识别 IP3108A 的过充、过放或短路状态，也不能替代该硬件保护。

关闭 VBAT_EN 不保证 USB 供电下的风机和雾化负载关闭，也不会直接断开 MCU 的 VBAT 电源。应用应在故障回调中停止负载 PWM，并管理后续恢复。

## 调用顺序

1. 在线程中调用 `bsp_gpio_Init()`，再调用 `bms_ctrl_Init(&config)` 并检查返回值。GPIO 模式错误或保护配置无效时初始化失败。
2. 每 1ms 在线程中调用 `bms_ctrl_1ms_task()`；它内部调用保护任务，不能再单独重复调用 `bms_protect_1ms_task()`。
3. 开启模拟量保护后，在主循环提交完整的新采样快照：`bms_protect_UpdateSample(&sample)`。ADC ISR 中先保存快照和就绪标记，由主循环读取；不要在 ISR 与线程并发修改模块状态。
4. 需要电池通路时调用 `bms_ctrl_SetVbatEnabled(true)`。默认初始化只请求 VIN，不自动接通电池或模拟 KEY。
5. 调用 `bms_ctrl_RequestKey(BMS_KEY_WAKEUP)` 等请求非阻塞按键动作；忙时返回 false，调用方应在后续任务中重试。

KEY 短按为 150ms，长按为 2500ms，双击为 150ms 按下、150ms 间隔、150ms 按下。每次动作后释放至少 1100ms，避免两次独立短按被识别为照明双击。应用仍需持续执行 1ms 任务以释放 KEY；动作完成前不能进入 STOP。

`bms_ctrl_EnterStandby()` 释放 KEY、保留 VIN 请求、关闭 VBAT 请求，不自动发送长按。若需要 IP5326 长按待机，应先请求 `BMS_KEY_STANDBY`，等待 `keyBusy` 变为 false，再进入本模块待机和系统 STOP。长按不会保证照明灯关闭，模块也没有照明状态反馈。

退出 STOP 后先恢复板级 GPIO。`bms_ctrl_LeaveStandby()` 仅清除待机请求；随后按业务需要重新请求 VBAT 和 KEY。保护状态应保留，不能通过反复初始化来清除运行中故障。

## 保护参数与采样

IO 操作、KEY/USB 时序、初始通路请求、故障屏蔽关系和默认保护参数统一放在 `Middleware/bms_ctrl/bms_config.h`，宏注释标明用途、单位和配置约束。引脚映射复用 `bsp_gpio.h`，避免与板级初始化不一致。

`bms_ctrl_Init(NULL)` / `bms_protect_Init(NULL)` 使用 `BMS_PROTECT_DEFAULT_*` 宏参数；显式传入 `bms_protect_config_t` 仍可覆盖默认值。资料未指定软件保护阈值，默认启用掩码为 0，阈值和保护时长的 0 为未配置占位；填写参数并设置启用掩码后，NULL 初始化会实际应用并校验宏配置。`enabledMask=0` 时不启用模拟量保护，仅支持上层显式报告的外部故障。产品阈值应由电芯、充电电路、负载和温升验证确定，不能使用主机测试中的数值。

| 保护位 | 采样单位 | 触发 / 恢复比较 | 输出联锁 |
| --- | --- | --- | --- |
| BATTERY_LOW | mV | ≤ 欠压阈值 / ≥ 恢复阈值 | 禁止 VBAT，允许 USB 充电 |
| BATTERY_HIGH | mV | ≥ 过压阈值 / ≤ 恢复阈值 | 禁止 VIN，允许电池放电 |
| USB_HIGH | mV | USB 存在且 ≥ 阈值 / ≤ 恢复阈值 | 禁止 VIN |
| TEMP_HIGH | 0.1℃ | ≥ 温度阈值 / ≤ 恢复阈值 | 禁止 VIN 和 VBAT |
| CURRENT_HIGH | mA | 负载开启且 ≥ 阈值 / ≤ 恢复阈值 | 禁止 VIN 和 VBAT |
| SAMPLE_INVALID | 有效位和数据年龄 | 缺少必需通道或采样超时 / 新有效采样 | 禁止 VIN 和 VBAT |
| EXTERNAL | 上层报告 | 显式置位 / 显式解除 | 禁止 VIN 和 VBAT |

配置通过 `enabledMask` 选择模拟量保护，通过 `latchMask` 选择需要人工解除的故障。已启用保护必须配置非零 `confirmMs`、`recoverMs` 和 `sampleTimeoutMs`，并提供严格分离的触发与恢复阈值。初始等待有效采样时同样禁止充放电；若锁存 SAMPLE_INVALID，需要在有效采样恢复后显式解除。

触发和恢复条件必须分别连续满足对应时长；进入滞回区间后保留当前故障状态。数据失效会立即关闭权限并重置相关判定计数。数据年龄在超时时饱和，避免计数回绕让旧数据重新有效。

`sample.validMask` 只能标记转换成功且可信的物理量。`sample.usbPresent` 应使用控制模块去抖后的状态；`sample.loadEnabled` 应反映应用实际负载状态。USB 不存在、负载关闭时不要求相应通道，但已触发的 USB/电流故障仍需有效测量才能恢复。

- 电池电压：当前 ADC 扫描没有直接电池电压通道，不从 LED 状态伪造电压。上层必须提供经过确认的电压采样来源，才能启用对应保护。
- USB 电压：USB 板 R24/R30 为 1M/100K，AD_USB 位于受 VIN_EN 控制的 VBUS 侧，分压倍率为 11；ADC 码需结合实际参考电压换算。
- 温度：主控板 NTC2 为 10K/B3435，R56 为 10K，USB 板 NTC1 由 IP3108A 自行检测。上层可使用现有 `g_ntc_WMF52D` 参数表，但应核对实际 NTC 型号和测量误差；开路、短路或转换失败时清除温度有效位。
- 电流：电机 R42/R43 为并联 100mΩ，雾化 R63/R64 为并联 0.5Ω，电池 R18 为 10mΩ。它们是不同测量路径；PGA2 的输入和偏置尚需确认，不能未经校准直接复用同一 ADC-to-mA 系数。

AD_USB 位于通路切断后的节点。USB 过压后若 VIN 关闭，其 ADC 读数降低不代表外部适配器恢复正常；启用 USB_HIGH 时配置必须同时锁存此位，否则初始化返回 false。应用应在拔出/重新确认输入后解除，防止自动反复接通。

`bms_protect_ClearLatched(mask)` 只清除已恢复的锁存位；仍在异常状态的故障不可清除。非锁存保护恢复后会按保留的通路请求自动恢复输出，应用如需禁止自动重启，应配置锁存并在故障回调中清除负载请求。

`bms_ctrl_FaultChangedCallback(faults)` 为线程上下文弱回调，仅在故障集合变化时调用。上层可以重写它停止风机、雾化 PWM，并记录故障。对硬件无反馈的 CTL、充满、升压成功等状态，模块不提供推测结果。

## 验证

模块实现时的主机 GPIO 模拟测试覆盖 KEY 时序、USB 去抖、待机、五种模拟量保护、触发/恢复边界、锁存、外部故障及数据失效/超时。本次配置宏迁移另以临时 GPIO 模拟测试验证默认宏、非零保护宏参数、显式配置覆盖和 IO 联锁，测试文件位于忽略目录 `Projects/App-Project/MDK-ARM/Objects/bms_review`。两个模块已通过 ArmClang Cortex-M0+ 语法检查和 ARMCC 对象编译。完整固件构建、实际保护阈值、CTL 功能及硬件波形仍需在系统接入后验证。
