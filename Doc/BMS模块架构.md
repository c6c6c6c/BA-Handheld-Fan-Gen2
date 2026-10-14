# BMS 模块架构

统一包含入口为 `Middleware/bms_ctrl/bms.h`，模块开关、IO 操作和默认参数均集中在 `bms_config.h`。配置头不反向包含业务头文件，避免循环依赖。

| 模块 | 配置宏（0 关闭 / 1 开启） | 职责 |
| --- | --- | --- |
| bms_ctrl | BMS_MODULE_CTRL_ENABLE | VIN/VBAT 通路请求、待机、保护联锁及子模块协调 |
| bms_key | BMS_MODULE_KEY_ENABLE | KEY 短按、长按、双击、释放 Guard 和动作取消 |
| bms_status | BMS_MODULE_STATUS_ENABLE | USB 插拔去抖及可选 1/2/4 灯模式识别 |
| bms_protect | BMS_MODULE_PROTECT_ENABLE | 物理量保护、采样超时、滞回恢复与故障锁存 |
| bms_soc | BMS_MODULE_SOC_ENABLE | 库仑计量、OCV 校正及满充/空电端点修正 |

默认全部开启，保持原有控制、保护和 SOC 能力。可以编辑配置头，也可以通过工程编译选项 `-DBMS_MODULE_KEY_ENABLE=0` 等覆盖开关。关闭模块后，其源文件可以保留在工程中（不生成实现），也可从工程中移除；其他启用模块不应引用它的函数。

各模块头文件保留基础数据类型，函数声明只在对应模块开启时可见。KEY 关闭时兼容入口 `bms_ctrl_RequestKey()` 保留，返回 false；`keyBusy` 为 false。STATUS 关闭时控制状态中的 USB/LED 字段为 false，`mode` 为 `BMS_STATUS_UNKNOWN`。PROTECT 关闭时 `faults=0`、不执行软件充放电联锁，初始化参数中的保护配置被忽略；这与只关闭模拟量保护阈值（`enabledMask=0`）不同，后者仍保留外部故障联锁。

`bms_config.h` 用 `BMS_STATUS_LED_MODE` 选择 `BMS_STATUS_MODE_1_LED`、`BMS_STATUS_MODE_2_LED`（本板默认）或 `BMS_STATUS_MODE_4_LED`。LED GPIO 读取均由 `BMS_IO_LEDx_IS_HIGH()` 宏封装，点亮电平由 `BMS_STATUS_LED_ON_IS_HIGH` 指定；去抖和周期容差也在此文件配置。1 灯模式只采集 LED1；0.5Hz 闪烁为充电中、1Hz 闪烁为低电量，常亮时结合去抖后的 USB 输入区分充满和正常放电。2 灯模式中 LED1 对应 D1、LED3 对应 D2，按两灯表识别四种状态。4 灯模式需先在 `bsp_gpio.h` 映射 LED2/LED4 并将其初始化为输入，之后识别充放电状态，同时给出 `solidLeds`（连续常亮灯数）和 `flashingLed`（闪烁灯号）；可按芯片四灯表解释区间。未确认的过渡组合返回 `BMS_STATUS_UNKNOWN`。显示状态不直接作为软件保护阈值。

## 协调模式（CTRL 开启）

1. 按启用的硬件功能配置 GPIO；通常先调用 `bsp_gpio_Init()`。
2. 调用 `bms_ctrl_Init(protectConfig)`。它只检查启用模块的 GPIO，并初始化 PROTECT、KEY、STATUS 中已启用的部分。
3. 每 1ms 调用一次 `bms_ctrl_1ms_task()`，内部按 STATUS → PROTECT → KEY → 通路更新的顺序协调各模块。不要再重复调用这些子模块的 1ms 任务。
4. 通过 `bms_ctrl_RequestKey(command)` 请求 KEY 动作；该入口检查控制层初始化状态、待机和保护权限，再转发 `bms_key_Request()`。原有调用方不需要修改命令枚举或入口名称。
5. 向 PROTECT 提交完整的新采样快照，方法与原实现一致。SOC 根据实际电池采样时间间隔调用 `bms_soc_update()`，不以 1ms 调度次数伪造采样或电量。

KEY 的状态、剩余时长、GPIO 驱动和转换逻辑全部由 `bms_key.c` 管理。控制层只调用 `bms_key_SetAllowed()` 注入业务权限、`bms_key_Cancel()` 处理待机取消，以及 `bms_key_GetStatus()` 同步忙状态。保护禁止放电时，非待机动作会立即释放 KEY 并进入 Guard，待机长按可以继续执行。

## 独立模式（CTRL 关闭）

应用分别初始化和调度已启用模块：

- KEY：`bms_key_Init()`、`bms_key_Request()`、`bms_key_1ms_task()`。模块默认允许动作，不自行读取保护或通路状态；应用必须通过 `bms_key_SetAllowed()` 注入业务权限，并按需取消动作。
- STATUS：`bms_status_Init()`、`bms_status_1ms_task()`、`bms_status_GetStatus()`。
- PROTECT：`bms_protect_Init()`、`bms_protect_UpdateSample()`、`bms_protect_1ms_task()`。此时应用自己消费保护权限并处理负载/通路，不会自动控制 GPIO。
- SOC：保持调用方持有 `BmsSocEstimator`，使用 `bms_soc_default_config()`、`bms_soc_init()` 和 `bms_soc_update()`；只开启算法模块时不依赖板级 GPIO 头文件。

上述 API 仍在线程上下文调用。动作完成前不能停止 KEY 的周期任务或进入 STOP。GPIO 实际初始化/休眠策略由 BSP 管理，关闭业务模块不会自动修改 BSP 的板级引脚配置。

SOC 默认参数已迁移到 `bms_config.h` 的 `BMS_SOC_DEFAULT_*` 宏，保持原数值。实际电池容量仍显式传给 `bms_soc_default_config()`，初始 SOC 和采样仍由上层提供；不自动假定电量。

## 验证

使用临时 GPIO 模拟测试对五个模块的全部 32 种开关组合进行编译、链接和运行。链接测试仅加入已启用模块的源文件，用于验证禁用模块没有隐藏符号依赖。测试覆盖 KEY 短按/长按/双击时序、Guard、权限取消、故障中断、USB 去抖、待机和 SOC 基本行为。全启用配置的五个模块通过 ARMCC 对象编译；全关闭配置通过 ARM 语法检查。新 KEY 和 STATUS 源文件已加入 Keil 工程，尚未进行硬件波形验证。
