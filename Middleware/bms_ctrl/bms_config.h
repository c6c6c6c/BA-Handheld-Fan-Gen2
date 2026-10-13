/**
 ***********************************************************************************************************************
 * @file    bms_config.h
 * @brief   BMS IO 适配、KEY 时序参数及软件保护默认配置
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. 本文件只负责 BMS 控制模块的板级 IO 适配及默认参数定义。
 *          2. 实际 GPIO 引脚映射统一复用 bsp_gpio.h，避免板级初始化与业务层操作使用不同引脚。
 *          3. VIN_EN / VBAT_EN / KEY 等控制均通过宏封装，业务层不直接操作 GPIO。
 *          4. 软件保护阈值默认未配置，启用前必须根据产品实际参数填写。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CONFIG_H_
#define _BMS_CONFIG_H_

#include "bsp_gpio.h"


/* IO Adapt ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief IP5326 KEY 控制接口
 *
 * 硬件连接：
 *
 * MCU PF3
 *   ↓
 * Q4 NPN
 *   ↓
 * IP5326 KEY
 *
 * MCU 输出高电平时 Q4 导通，将 IP5326 KEY 拉低，
 * 相当于模拟一次实体按键“按下”。
 *
 * MCU 输出低电平时 Q4 截止，释放 IP5326 KEY，
 * 相当于模拟实体按键“释放”。
 *
 * 因此这里的业务逻辑为：
 *
 * BMS_IO_KEY_PRESS()   -> MCU KEY 输出高
 * BMS_IO_KEY_RELEASE() -> MCU KEY 输出低
 */
#define BMS_IO_KEY_PRESS()          LL_GPIO_SetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)
#define BMS_IO_KEY_RELEASE()        LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)


/**
 * @brief VIN 输入通路控制
 *
 * VIN_EN 为高有效控制信号。
 *
 * 硬件作用：
 *
 * VIN_EN
 *   ↓
 * Q3
 *   ↓
 * 控制输入侧 MOS
 *   ↓
 * VIN-USB -> IP5326 VBUS
 *
 * VIN_EN = 高：
 * 开启 USB/VIN 输入通路。
 *
 * VIN_EN = 低：
 * 关闭 USB/VIN 输入通路。
 */
#define BMS_IO_VIN_ENABLE()         LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)
#define BMS_IO_VIN_DISABLE()        LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN)


/**
 * @brief VBAT 电池通路控制
 *
 * VBAT_EN 为高有效控制信号。
 *
 * 硬件作用：
 *
 * VBAT_EN
 *   ↓
 * Q5
 *   ↓
 * 控制电池侧 MOS
 *   ↓
 * BATTERY -> VBAT / IP5326 BAT
 *
 * VBAT_EN = 高：
 * 开启电池输出通路。
 *
 * VBAT_EN = 低：
 * 关闭电池输出通路。
 *
 * @note
 * 该控制不等价于直接切断 MCU 自身供电，
 * 实际断开的只是对应电池功率通路。
 */
#define BMS_IO_VBAT_ENABLE()        LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)
#define BMS_IO_VBAT_DISABLE()       LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN)


/**
 * @brief BMS CTL 电平控制
 *
 * CTL 为外部 BMS / 保护 IC 控制引脚。
 *
 * 当前 CTL 有效极性及具体功能尚未最终确认，
 * 因此这里只提供原始高低电平操作接口，
 * bms_ctrl 模块不会自动控制 CTL。
 *
 * 使用前应根据对应器件手册确认：
 *
 * CTL = HIGH / LOW 分别代表什么功能。
 */
#define BMS_IO_CTL_HIGH()           LL_GPIO_SetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN)
#define BMS_IO_CTL_LOW()            LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN)


/**
 * @brief USB 输入状态检测
 *
 * USB_IN 高电平表示 USB 输入存在。
 *
 * USB_IN 可用于：
 *
 * 1. USB 插入检测
 * 2. USB 唤醒
 * 3. 上层 USB 状态判断
 *
 * @note
 * 这里只读取数字电平，不进行 USB 电压 ADC 换算。
 */
#define BMS_IO_USB_IS_PRESENT()     (LL_GPIO_IsInputPinSet(BMS_USB_IN_PORT, BMS_USB_IN_PIN) != 0U)


/**
 * @brief IP5326 LED1 原始电平读取
 *
 * 返回 MCU 采集到的 LED1 引脚当前逻辑电平。
 *
 * @note
 * 返回值只代表 GPIO 高低状态，
 * 不直接等价于电池剩余百分比。
 */
#define BMS_IO_LED1_IS_HIGH()       (LL_GPIO_IsInputPinSet(BMS_LED1_PORT, BMS_LED1_PIN) != 0U)


/**
 * @brief IP5326 LED3 原始电平读取
 *
 * 返回 MCU 采集到的 LED3 引脚当前逻辑电平。
 *
 * @note
 * 当前硬件仅采集 LED1 / LED3，
 * 不应直接套用 IP5326 完整四灯电量显示表。
 */
#define BMS_IO_LED3_IS_HIGH()       (LL_GPIO_IsInputPinSet(BMS_LED3_PORT, BMS_LED3_PIN) != 0U)


/**
 * @brief 检查 BMS 控制相关 GPIO 是否已正确初始化
 *
 * 本宏只检查 GPIO 模式是否符合预期，不会主动修改 GPIO 配置。
 *
 * 检查内容：
 *
 * KEY      -> OUTPUT
 * VIN_EN   -> OUTPUT
 * VBAT_EN  -> OUTPUT
 * USB_IN   -> INPUT
 * LED1     -> INPUT
 * LED3     -> INPUT
 *
 * @note
 * 应在 bsp_gpio_Init() 完成之后调用。
 *
 * @note
 * CTL 当前不参与 bms_ctrl 自动业务控制，因此不纳入检查。
 */
#define BMS_IO_IS_READY() \
    ((LL_GPIO_GetPinMode(BMS_KEY_PORT,     BMS_KEY_PIN)     == LL_GPIO_MODE_OUTPUT) && \
     (LL_GPIO_GetPinMode(BMS_VIN_EN_PORT,  BMS_VIN_EN_PIN)  == LL_GPIO_MODE_OUTPUT) && \
     (LL_GPIO_GetPinMode(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN) == LL_GPIO_MODE_OUTPUT) && \
     (LL_GPIO_GetPinMode(BMS_USB_IN_PORT,  BMS_USB_IN_PIN)  == LL_GPIO_MODE_INPUT)  && \
     (LL_GPIO_GetPinMode(BMS_LED1_PORT,    BMS_LED1_PIN)    == LL_GPIO_MODE_INPUT)  && \
     (LL_GPIO_GetPinMode(BMS_LED3_PORT,    BMS_LED3_PIN)    == LL_GPIO_MODE_INPUT))


/* Control Parameters ------------------------------------------------------------------------------------------------*/

/**
 * @brief IP5326 KEY 短按持续时间
 *
 * 单位：ms
 *
 * 根据 KEY 时序要求：
 * 必须大于 100ms，且小于 2000ms。
 *
 * 当前设置 150ms，留有足够裕量，避免卡在临界时间。
 */
#define BMS_CTRL_KEY_SHORT_MS        150U


/**
 * @brief IP5326 KEY 长按持续时间
 *
 * 单位：ms
 *
 * 根据 KEY 时序要求：
 * 长按必须大于 2000ms。
 *
 * 当前使用 2500ms，用于触发 Standby 类长按动作。
 */
#define BMS_CTRL_KEY_LONG_MS         2500U


/**
 * @brief 双击操作两次短按之间的释放间隔
 *
 * 单位：ms
 *
 * 用于：
 *
 * 第一次短按
 *   ↓
 * 释放 KEY
 *   ↓
 * 等待 BMS_CTRL_KEY_DOUBLE_GAP_MS
 *   ↓
 * 第二次短按
 *
 * 当前设置为 150ms。
 */
#define BMS_CTRL_KEY_DOUBLE_GAP_MS   150U


/**
 * @brief KEY 操作完成后的释放保护时间
 *
 * 单位：ms
 *
 * 防止连续调用 KEY 命令时，被 IP5326 误识别成双击。
 *
 * 当前要求 > 1000ms。
 */
#define BMS_CTRL_KEY_GUARD_MS        1100U


/**
 * @brief USB 插入/拔出数字输入去抖时间
 *
 * 单位：ms
 *
 * USB_IN 必须连续保持同一状态达到该时间，
 * 才会更新 bms_ctrl_status_t.usbPresent。
 */
#define BMS_CTRL_USB_DEBOUNCE_MS     20U


/**
 * @brief BMS 初始化后默认 VIN 输入请求状态
 *
 * true：
 * 初始化后请求保持 USB/VIN 输入通路开启。
 *
 * @note
 * 最终是否真正打开 VIN_EN，
 * 仍受 bms_protect 的 chargeAllowed 条件限制。
 */
#define BMS_CTRL_INIT_VIN_ENABLED    true


/**
 * @brief BMS 初始化后默认 VBAT 电池通路请求状态
 *
 * false：
 * 初始化后不自动开启电池输出通路，
 * 避免设备初始化过程中负载提前启动。
 */
#define BMS_CTRL_INIT_VBAT_ENABLED   false


/**
 * @brief 进入待机状态时 VIN 输入通路请求状态
 *
 * true：
 * 待机期间保留 USB 输入路径，
 * 允许 USB 插入检测及外部充电。
 */
#define BMS_CTRL_STANDBY_VIN_ENABLED true


/**
 * @brief KEY 时序及 USB 去抖参数编译期合法性检查
 *
 * 主要检查：
 *
 * 1. KEY 短按时间满足 100ms < short < 2000ms
 * 2. KEY 长按时间 > 2000ms
 * 3. 所有 uint16_t 时间参数不超过 65535ms
 * 4. 双击整体动作时间 < 1000ms
 * 5. Guard 时间 > 1000ms
 * 6. USB debounce 时间非 0
 *
 * 如果任一参数不符合要求，直接在编译阶段报错。
 */
#if (BMS_CTRL_KEY_SHORT_MS <= 100U)                                      || \
    (BMS_CTRL_KEY_SHORT_MS >= 2000U)                                     || \
    (BMS_CTRL_KEY_LONG_MS  <= 2000U)                                     || \
    (BMS_CTRL_KEY_LONG_MS  >  65535U)                                    || \
    (BMS_CTRL_KEY_DOUBLE_GAP_MS == 0U)                                   || \
    ((2U * BMS_CTRL_KEY_SHORT_MS + BMS_CTRL_KEY_DOUBLE_GAP_MS) >= 1000U) || \
    (BMS_CTRL_KEY_GUARD_MS <= 1000U)                                     || \
    (BMS_CTRL_KEY_GUARD_MS > 65535U)                                     || \
    (BMS_CTRL_USB_DEBOUNCE_MS == 0U)                                     || \
    (BMS_CTRL_USB_DEBOUNCE_MS > 65535U)

#error "Invalid BMS KEY timing or USB debounce configuration"

#endif


/* Protection Type ---------------------------------------------------------------------------------------------------*/

/**
 * @brief 模拟量保护索引
 *
 * 这些索引同时用于：
 *
 * 1. 故障位偏移
 * 2. 触发计数器数组索引
 * 3. 恢复计数器数组索引
 *
 * 因此必须从 0 开始连续编号。
 */
#define BMS_PROTECT_BATTERY_LOW_INDEX    0U
#define BMS_PROTECT_BATTERY_HIGH_INDEX   1U
#define BMS_PROTECT_USB_HIGH_INDEX       2U
#define BMS_PROTECT_TEMP_HIGH_INDEX      3U
#define BMS_PROTECT_CURRENT_HIGH_INDEX   4U

/**
 * @brief 可配置模拟量保护类型数量
 */
#define BMS_PROTECT_ANALOG_COUNT         5U


/**
 * @brief 电池欠压故障
 *
 * 当 batteryMv 低于配置的欠压触发阈值并持续达到确认时间后置位。
 */
#define BMS_PROTECT_BATTERY_LOW     (1UL << BMS_PROTECT_BATTERY_LOW_INDEX)


/**
 * @brief 电池过压故障
 */
#define BMS_PROTECT_BATTERY_HIGH     (1UL << BMS_PROTECT_BATTERY_HIGH_INDEX)


/**
 * @brief USB 输入过压故障
 */
#define BMS_PROTECT_USB_HIGH         (1UL << BMS_PROTECT_USB_HIGH_INDEX)


/**
 * @brief 主控板 NTC 过温故障
 */
#define BMS_PROTECT_TEMP_HIGH        (1UL << BMS_PROTECT_TEMP_HIGH_INDEX)


/**
 * @brief 负载电流过高故障
 */
#define BMS_PROTECT_CURRENT_HIGH     (1UL << BMS_PROTECT_CURRENT_HIGH_INDEX)


/**
 * @brief 采样数据无效故障
 *
 * 典型情况：
 *
 * - 必须的 ADC 数据从未更新
 * - 数据更新时间超过 sampleTimeoutMs
 * - 温度采样无效
 * - 电流/电压采样未准备完成
 */
#define BMS_PROTECT_SAMPLE_INVALID   (1UL << BMS_PROTECT_ANALOG_COUNT)


/**
 * @brief 上层主动报告的外部故障
 *
 * 用于 bms_protect 内部无法自动检测，
 * 但上层业务希望统一纳入保护联锁的故障。
 */
#define BMS_PROTECT_EXTERNAL         (BMS_PROTECT_SAMPLE_INVALID << 1)


/**
 * @brief 所有模拟量自动保护故障位掩码
 *
 * 包含：
 *
 * BATTERY_LOW
 * BATTERY_HIGH
 * USB_HIGH
 * TEMP_HIGH
 * CURRENT_HIGH
 */
#define BMS_PROTECT_ANALOG_MASK     (BMS_PROTECT_SAMPLE_INVALID - 1UL)


/**
 * @brief 当前软件支持的全部故障位
 *
 * 包括：
 *
 * 模拟量保护
 * 采样无效
 * 外部故障
 */
#define BMS_PROTECT_ALL_MASK        ((BMS_PROTECT_EXTERNAL << 1) - 1UL)


/* Sample Valid Flags ------------------------------------------------------------------------------------------------*/

/**
 * @brief batteryMv 测量值有效
 */
#define BMS_SAMPLE_BATTERY          (1UL << 0)

/**
 * @brief usbMv 测量值有效
 */
#define BMS_SAMPLE_USB              (1UL << 1)

/**
 * @brief temperatureDeciC 温度值有效
 *
 * 要求：
 *
 * - NTC ADC 数据有效
 * - NTC 无开路
 * - NTC 无短路
 * - 温度换算成功
 */
#define BMS_SAMPLE_TEMPERATURE      (1UL << 2)

/**
 * @brief currentMa 电流测量值有效
 *
 * 表示当前电流值已经按照实际采样电阻及放大倍数完成校准。
 */
#define BMS_SAMPLE_CURRENT          (1UL << 3)


/* Protection Interlock ----------------------------------------------------------------------------------------------*/

/**
 * @brief 禁止充电的故障集合
 *
 * 当以下任意故障存在时：
 *
 * chargeAllowed = false
 *
 * 包括：
 *
 * - 电池过压
 * - USB 输入过压
 * - 过温
 * - 过流
 * - 采样无效
 * - 外部故障
 */
#define BMS_PROTECT_CHARGE_BLOCK_MASK \
    (BMS_PROTECT_BATTERY_HIGH       | \
     BMS_PROTECT_USB_HIGH           | \
     BMS_PROTECT_TEMP_HIGH          | \
     BMS_PROTECT_CURRENT_HIGH       | \
     BMS_PROTECT_SAMPLE_INVALID     | \
     BMS_PROTECT_EXTERNAL)


/**
 * @brief 禁止放电的故障集合
 *
 * 当以下任意故障存在时：
 *
 * dischargeAllowed = false
 *
 * 包括：
 *
 * - 电池欠压
 * - 过温
 * - 过流
 * - 采样无效
 * - 外部故障
 *
 * @note
 * 电池过压和 USB 过压通常不会直接禁止电池放电，
 * 因此没有包含在该掩码中。
 */
#define BMS_PROTECT_DISCHARGE_BLOCK_MASK  \
            (BMS_PROTECT_BATTERY_LOW    | \
            BMS_PROTECT_TEMP_HIGH       | \
            BMS_PROTECT_CURRENT_HIGH    | \
            BMS_PROTECT_SAMPLE_INVALID  | \
            BMS_PROTECT_EXTERNAL)


/* Default Protection Parameters -------------------------------------------------------------------------------------*/

/**
 * @brief 默认保护参数说明
 *
 * 当前尚未确认正式产品保护阈值，因此默认：
 *
 * enabledMask = 0
 *
 * 即所有模拟量自动保护默认关闭。
 *
 * 以下值中的 0 只是“未配置占位值”，
 * 不是有效的产品保护阈值。
 *
 * 如果后续启用某一项保护，必须同步配置：
 *
 * 1. 触发阈值
 * 2. 恢复阈值
 * 3. confirmMs
 * 4. recoverMs
 * 5. sampleTimeoutMs
 *
 * bms_protect_Init() 应负责检查配置合法性。
 */


/**
 * @brief 默认启用的模拟量保护位
 *
 * 0 表示默认全部关闭。
 */
#define BMS_PROTECT_DEFAULT_ENABLED_MASK                0UL


/**
 * @brief 默认锁存故障位
 *
 * 锁存故障在条件恢复后不会自动清除，
 * 需要显式清除或重新初始化。
 */
#define BMS_PROTECT_DEFAULT_LATCH_MASK                  0UL


/**
 * @brief 故障触发确认时间
 *
 * 单位：ms
 *
 * 信号必须连续超过阈值达到该时间，
 * 才确认故障成立。
 */
#define BMS_PROTECT_DEFAULT_CONFIRM_MS                  0U


/**
 * @brief 故障恢复确认时间
 *
 * 单位：ms
 *
 * 信号必须连续恢复至安全范围达到该时间，
 * 才清除非锁存故障。
 */
#define BMS_PROTECT_DEFAULT_RECOVER_MS                  0U


/**
 * @brief 最大允许采样数据年龄
 *
 * 单位：ms
 *
 * 如果采样数据超过该时间没有刷新，
 * 可置位 BMS_PROTECT_SAMPLE_INVALID。
 */
#define BMS_PROTECT_DEFAULT_SAMPLE_TIMEOUT_MS           0U


/**
 * @brief 电池欠压触发阈值，单位 mV
 */
#define BMS_PROTECT_DEFAULT_BATTERY_LOW_MV              0U


/**
 * @brief 电池欠压恢复阈值，单位 mV
 *
 * 必须：
 *
 * BATTERY_LOW_RECOVER_MV > BATTERY_LOW_MV
 *
 * 用于形成欠压恢复迟滞。
 */
#define BMS_PROTECT_DEFAULT_BATTERY_LOW_RECOVER_MV      0U


/**
 * @brief 电池过压触发阈值，单位 mV
 */
#define BMS_PROTECT_DEFAULT_BATTERY_HIGH_MV             0U


/**
 * @brief 电池过压恢复阈值，单位 mV
 *
 * 必须：
 *
 * BATTERY_HIGH_RECOVER_MV < BATTERY_HIGH_MV
 */
#define BMS_PROTECT_DEFAULT_BATTERY_HIGH_RECOVER_MV     0U


/**
 * @brief USB 输入过压触发阈值，单位 mV
 *
 * 测量对象为 VBUS / USB 输入电压。
 */
#define BMS_PROTECT_DEFAULT_USB_HIGH_MV                 0U


/**
 * @brief USB 输入过压恢复阈值，单位 mV
 *
 * 必须小于 USB 过压触发阈值。
 */
#define BMS_PROTECT_DEFAULT_USB_HIGH_RECOVER_MV         0U


/**
 * @brief NTC 过温触发阈值
 *
 * 单位：0.1 摄氏度
 *
 * 例如：
 *
 * 650 -> 65.0°C
 */
#define BMS_PROTECT_DEFAULT_TEMP_HIGH_DECI_C            0


/**
 * @brief NTC 过温恢复阈值
 *
 * 单位：0.1 摄氏度
 *
 * 必须小于过温触发阈值，
 * 用于形成温度恢复迟滞。
 */
#define BMS_PROTECT_DEFAULT_TEMP_RECOVER_DECI_C         0


/**
 * @brief 负载过流触发阈值，单位 mA
 */
#define BMS_PROTECT_DEFAULT_CURRENT_HIGH_MA             0U


/**
 * @brief 负载过流恢复阈值，单位 mA
 *
 * 必须小于过流触发阈值。
 */
#define BMS_PROTECT_DEFAULT_CURRENT_RECOVER_MA          0U


/**
 * @brief 默认 BMS 软件保护配置初始化器
 *
 * 用于：
 *
 * bms_protect_config_t config = BMS_PROTECT_DEFAULT_CONFIG;
 *
 * 或在 bms_protect_Init(NULL) 时作为内部默认配置来源。
 *
 * 字段顺序与 bms_protect_config_t 保持一致。
 */
#define BMS_PROTECT_DEFAULT_CONFIG \
    { \
        .enabledMask             = BMS_PROTECT_DEFAULT_ENABLED_MASK,            \
        .latchMask               = BMS_PROTECT_DEFAULT_LATCH_MASK,              \
        .confirmMs               = BMS_PROTECT_DEFAULT_CONFIRM_MS,              \
        .recoverMs               = BMS_PROTECT_DEFAULT_RECOVER_MS,              \
        .sampleTimeoutMs         = BMS_PROTECT_DEFAULT_SAMPLE_TIMEOUT_MS,       \
        .batteryLowMv            = BMS_PROTECT_DEFAULT_BATTERY_LOW_MV,          \
        .batteryLowRecoverMv     = BMS_PROTECT_DEFAULT_BATTERY_LOW_RECOVER_MV,  \
        .batteryHighMv           = BMS_PROTECT_DEFAULT_BATTERY_HIGH_MV,         \
        .batteryHighRecoverMv    = BMS_PROTECT_DEFAULT_BATTERY_HIGH_RECOVER_MV, \
        .usbHighMv               = BMS_PROTECT_DEFAULT_USB_HIGH_MV,             \
        .usbHighRecoverMv        = BMS_PROTECT_DEFAULT_USB_HIGH_RECOVER_MV,     \
        .temperatureHighDeciC    = BMS_PROTECT_DEFAULT_TEMP_HIGH_DECI_C,        \
        .temperatureRecoverDeciC = BMS_PROTECT_DEFAULT_TEMP_RECOVER_DECI_C,     \
        .currentHighMa           = BMS_PROTECT_DEFAULT_CURRENT_HIGH_MA,         \
        .currentRecoverMa        = BMS_PROTECT_DEFAULT_CURRENT_RECOVER_MA       \
    }


#endif /* _BMS_CONFIG_H_ */
