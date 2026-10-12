# BA Handheld Fan Gen2 固件工程

本仓库是第二代手持风扇控制板的嵌入式固件工程，目标芯片为 **PY32F032x8（Cortex-M0+）**，使用 Keil MDK-ARM 与 PY32F0xx LL 驱动开发。

> 当前仓库处于基础工程搭建阶段。可构建的 `App-Project` 已完成系统时钟初始化，但主循环为空；风扇、按键、LED、BMS、Modbus 等业务源码尚未加入当前 Keil Target，不能将仓库中存在这些源码理解为功能已经集成。

## 当前工程状态

| 项目 | 当前实际配置 |
| --- | --- |
| MCU | PY32F032x8，Cortex-M0+ |
| Flash | 64 KiB，`0x08000000`～`0x0800FFFF` |
| RAM | 8 KiB，`0x20000000`～`0x20001FFF` |
| 系统时钟 | `main.c` 使用 HSI/HSISYS，并将 HCLK、PCLK 配置为 72 MHz |
| 工程 | `Projects/App-Project/MDK-ARM/Project.uvprojx` |
| Target | `Project` |
| 编译器 | Arm Compiler 5.06 update 7（build 960） |
| 芯片支持包 | 工程引用 `Puya.PY32F0xx_DFP 1.2.20` |
| 输出格式 | AXF 与 HEX；工程当前未配置 After Build 生成 BIN |

当前 Target 实际包含：

- 启动文件、系统文件、`main.c` 和中断处理文件；
- PY32F032 LL 外设驱动；
- `Middleware/common/timer_base`；
- `Middleware/common/port`。

当前程序启动后只完成系统时钟和 SysTick 配置，随后进入空循环。`SysTick_Handler()` 也是空实现，因此 `timer_base` 的 `SystemMsUpdate()` 目前没有被周期调用，软件时间基准尚未工作。

## 目录结构

```text
BA_handheld_Fan_Gen2/
├─ Projects/
│  ├─ App-Project/
│  │  ├─ Inc/                    # 应用头文件与工程文件清单
│  │  ├─ Src/                    # main、系统文件和中断入口
│  │  └─ MDK-ARM/                # Keil 工程及构建输出
│  └─ Output/                    # 预留的固件输出目录
├─ Middleware/
│  ├─ common/                    # 按键、风扇、LED、参数、休眠、端口、时间基准
│  ├─ protocol/                  # BMS 与 Modbus 协议源码
│  ├─ algorithm/                 # CRC、XXTEA
│  └─ data_structure/            # 队列实现
├─ Drivers/
│  ├─ CMSIS/
│  ├─ PY32F032_HAL_Driver/       # PY32F032 HAL/LL 驱动
│  └─ BSP/                       # PY32F032 Start Kit BSP
├─ Other/
│  ├─ 原理图/                    # 主控板、显示板、按键板等硬件原理图
│  ├─ 芯片手册/                  # PY32F032 数据手册
│  └─ Packs/MDK/                 # 仓库内附的旧版 DFP 安装包
└─ Tools/
   ├─ jlink/                     # J-Link 擦除、烧录和复位脚本
   └─ merge_bin/                 # BIN 合并与升级头生成脚本
```

## 开发环境

建议使用与现有工程最近的环境：

1. Keil MDK 5（已有构建日志使用 μVision 5.33）；
2. Arm Compiler 5.06 update 7；
3. ARM CMSIS 5.7.0；
4. Puya PY32F0xx DFP 1.2.20。

`Other/Packs/MDK/Puya.PY32F0xx_DFP.1.2.6.pack` 是仓库内附的 1.2.6 版本，而 `Project.uvprojx` 当前引用 1.2.20。若 Keil 提示缺少 Device Pack，应安装 1.2.20，或在确认兼容性后再修改工程引用版本。

## 构建方法

1. 使用 Keil 打开：

   ```text
   Projects/App-Project/MDK-ARM/Project.uvprojx
   ```

2. 选择 Target `Project`。
3. 执行 **Build Target** 或 **Rebuild all target files**。
4. 构建产物位于：

   ```text
   Projects/App-Project/MDK-ARM/Output/
   ├─ Project.axf
   ├─ Project.hex
   ├─ Project.map
   └─ Project.build_log.htm
   ```

仓库中保留的最近一次构建日志记录为 `0 Error(s), 0 Warning(s)`，程序占用为 `Code=504`、`RO-data=224`、`RW-data=4`、`ZI-data=1028`。该数值只代表当前基础框架，后续集成业务模块后会变化。

## 中间件说明

以下模块源码已经存在，但除 `common/port` 和 `common/timer_base` 外，当前均未加入 Keil Target：

| 目录 | 作用 |
| --- | --- |
| `Middleware/common/button` | 按键消抖及短按、长按事件识别 |
| `Middleware/common/fan` | 风扇档位、转速反馈和超时检测 |
| `Middleware/common/led` | LED 开关、亮度、呼吸灯及软件 PWM |
| `Middleware/common/param` | Flash 参数、按键记录、故障记录和 OTA 状态 |
| `Middleware/common/sleep` | 系统休眠及 BMS 唤醒管理 |
| `Middleware/protocol/bms` | BMS 通信及升级相关逻辑 |
| `Middleware/protocol/modbus` | Modbus RTU 主站、寄存器和消息队列 |
| `Middleware/algorithm` | CRC 与 XXTEA 算法 |
| `Middleware/data_structure` | 字节队列 |

集成这些模块时，需要同时补齐对应的 BSP/硬件接口、头文件搜索路径、初始化流程、中断回调和周期任务，不能只把单个 `.c` 文件加入工程。

## 烧录与工具现状

当前建议直接通过 Keil 下载 `Project.axf`/`Project.hex` 到目标板。

`Tools/jlink`、`Tools/merge_bin` 和 `.vscode/tasks.json` 是从旧的 **D940v2 / PY32F030 Boot+App** 工程遗留的工具配置，目前与本工程并不匹配，主要差异包括：

- 工具配置引用 `Project/Output`、`D940v2_boot.bin` 和 `D940v2_app.bin`，当前工程实际目录为 `Projects/App-Project/...`，且没有 Boot 工程；
- 合并脚本假设 App 起始地址为 `0x08004000`，当前 Keil 工程链接地址为 `0x08000000`；
- VS Code 任务中的器件名仍为 `PY32F030x8`；
- 当前 Keil Target 只生成 HEX，没有生成合并脚本所需的 BIN；
- 仓库中的 VS Code 任务文本存在编码损坏，不能作为已验证的构建/烧录入口。

在重新确定 Bootloader、Flash 分区、升级包格式和产物路径之前，请勿使用这些遗留任务烧录量产设备。若只想使用 `jlink_flash.ps1`，也必须显式传入已经确认过的 BIN、烧录地址和器件型号。

## 硬件资料

`Other/原理图` 中包含以下板卡资料：

- 主控板；
- 显示板；
- 按键板；
- 拨码开关板；
- USB 板。

芯片资料位于 `Other/芯片手册/PY32F032系列数据手册_V0.9.pdf`。

## 后续集成建议

1. 根据主控板原理图建立项目专用 BSP，并确定 GPIO、PWM、捕获、UART、ADC 与 Flash 分区；
2. 在 `main.c` 中加入外设和业务模块初始化；
3. 在 SysTick 或其他 1 ms 定时中断中维护系统时基，并调度各模块的周期任务；
4. 按依赖关系逐步加入风扇、按键、显示、BMS 和 Modbus 模块；
5. 完成功能集成后，再统一修正 BIN 生成、固件合并、J-Link 脚本和 VS Code 任务。

PA11 -> TIM1_CH4   -> FAN FG
PA3  -> TIM1_CH1   -> FAN PWM
PB6  -> TIM16_CH1N -> WH  PWM
