/**
 ***********************************************************************************************************************
 * @file    bms_protect.h
 * @brief   BMS 软件保护接口定义，实现物理量输入、延时判定、滞回恢复及故障锁存
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. 本模块属于软件保护层，不替代 IP3108A 等硬件电池保护。
 *          2. 支持电池欠压/过压、USB 过压、过温、过流、采样无效及外部故障。
 *          3. 模拟量保护支持触发延时、恢复延时、恢复滞回及故障锁存。
 *          4. ADC ISR 不应直接调用保护任务，建议由主循环或周期任务提交完整采样快照。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_PROTECT_H_
#define _BMS_PROTECT_H_

#include <stdbool.h>
#include <stdint.h>
#include "bms_config.h"


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 软件保护配置参数
 *
 * 用于配置各项模拟量保护的使能状态、锁存策略、
 * 触发/恢复确认时间以及对应物理量阈值。
 *
 * @note
 * enabledMask 为 0 时表示不启用任何模拟量自动保护，
 * 但仍可通过 bms_protect_SetExternalFault() 使用外部故障联锁。
 */
typedef struct
{
    /**
     * @brief 启用的模拟量保护位
     *
     * 可由以下保护位组合：
     *
     * - BMS_PROTECT_BATTERY_LOW
     * - BMS_PROTECT_BATTERY_HIGH
     * - BMS_PROTECT_USB_HIGH
     * - BMS_PROTECT_TEMP_HIGH
     * - BMS_PROTECT_CURRENT_HIGH
     *
     * 0 表示关闭所有模拟量自动保护。
     */
    uint32_t enabledMask;

    /**
     * @brief 故障锁存位配置
     *
     * 对应故障一旦触发后，即使实时条件恢复，
     * 仍保留在 latchedFaults 中，
     * 必须通过 bms_protect_ClearLatched() 显式清除。
     *
     * @note
     * 当前设计中启用 USB_HIGH 时要求同时配置为锁存。
     */
    uint32_t latchMask;

    /**
     * @brief 故障触发连续确认时间
     *
     * 单位：ms
     *
     * 某保护条件必须连续成立达到该时间，
     * 才正式置位对应 activeFault。
     *
     * 启用任意模拟量保护时必须 > 0。
     */
    uint16_t confirmMs;

    /**
     * @brief 故障恢复连续确认时间
     *
     * 单位：ms
     *
     * 某保护恢复条件必须连续成立达到该时间，
     * 才清除对应 activeFault。
     *
     * 启用任意模拟量保护时必须 > 0。
     */
    uint16_t recoverMs;

    /**
     * @brief 采样数据最大允许年龄
     *
     * 单位：ms
     *
     * 如果超过该时间未收到新的采样快照，
     * 当前采样会被视为无效并触发 SAMPLE_INVALID。
     */
    uint16_t sampleTimeoutMs;

    /**
     * @brief 电池欠压触发阈值
     *
     * 单位：mV
     */
    uint16_t batteryLowMv;

    /**
     * @brief 电池欠压恢复阈值
     *
     * 单位：mV
     *
     * 必须：
     *
     * batteryLowRecoverMv > batteryLowMv
     */
    uint16_t batteryLowRecoverMv;

    /**
     * @brief 电池过压触发阈值
     *
     * 单位：mV
     */
    uint16_t batteryHighMv;

    /**
     * @brief 电池过压恢复阈值
     *
     * 单位：mV
     *
     * 必须：
     *
     * batteryHighRecoverMv < batteryHighMv
     */
    uint16_t batteryHighRecoverMv;

    /**
     * @brief USB 输入过压触发阈值
     *
     * 单位：mV
     */
    uint16_t usbHighMv;

    /**
     * @brief USB 输入过压恢复阈值
     *
     * 单位：mV
     *
     * 必须：
     *
     * usbHighRecoverMv < usbHighMv
     */
    uint16_t usbHighRecoverMv;

    /**
     * @brief 过温触发温度
     *
     * 单位：0.1°C
     *
     * 例如：
     *
     * 650 -> 65.0°C
     */
    int16_t temperatureHighDeciC;

    /**
     * @brief 过温恢复温度
     *
     * 单位：0.1°C
     *
     * 必须：
     *
     * temperatureRecoverDeciC < temperatureHighDeciC
     */
    int16_t temperatureRecoverDeciC;

    /**
     * @brief 负载过流触发阈值
     *
     * 单位：mA
     */
    uint16_t currentHighMa;

    /**
     * @brief 负载过流恢复阈值
     *
     * 单位：mA
     *
     * 必须：
     *
     * currentRecoverMa < currentHighMa
     */
    uint16_t currentRecoverMa;

} bms_protect_config_t;


/**
 * @brief BMS 软件保护采样快照
 *
 * 上层应将同一时刻或尽量接近同一时刻的电压、温度、电流及状态信息
 * 组合成一份完整快照后，通过 bms_protect_UpdateSample() 提交。
 */
typedef struct
{
    /**
     * @brief 当前采样中有效的物理量位
     *
     * 可由以下位组合：
     *
     * - BMS_SAMPLE_BATTERY
     * - BMS_SAMPLE_USB
     * - BMS_SAMPLE_TEMPERATURE
     * - BMS_SAMPLE_CURRENT
     *
     * ADC 转换失败、NTC 开短路、换算失败等情况，
     * 应清除对应有效位。
     */
    uint32_t validMask;

    /**
     * @brief 当前电池电压
     *
     * 单位：mV
     *
     * @note
     * 当前 ADC 没有直接电池电压采样通道，
     * 需要由上层根据实际硬件测量方式提供。
     */
    uint16_t batteryMv;

    /**
     * @brief 当前 USB 输入电压
     *
     * 单位：mV
     *
     * @note
     * 应由上层完成 ADC 原始值到实际输入电压的换算后再提交。
     */
    uint16_t usbMv;

    /**
     * @brief 当前温度
     *
     * 单位：0.1°C
     *
     * @note
     * 当前保护使用的是主控板 NTC2 采样结果，
     * 不是 USB 板上的 NTC1。
     */
    int16_t temperatureDeciC;

    /**
     * @brief 当前负载电流
     *
     * 单位：mA
     *
     * @note
     * 该值必须已经按照实际采样电阻、
     * 运放增益及 ADC 量程完成校准。
     */
    uint16_t currentMa;

    /**
     * @brief USB 当前是否处于插入状态
     *
     * true：
     * USB 已经过数字输入去抖并确认存在。
     *
     * false：
     * USB 当前未确认插入。
     *
     * 用于 USB_HIGH 的触发条件判断。
     */
    bool usbPresent;

    /**
     * @brief 当前负载是否处于启用状态
     *
     * true：
     * 当前存在需要进行过流监测的负载。
     *
     * false：
     * 当前负载关闭。
     *
     * 用于 CURRENT_HIGH 的触发条件判断。
     */
    bool loadEnabled;

} bms_protect_sample_t;


/**
 * @brief BMS 软件保护当前状态
 *
 * 保存实时故障、锁存故障、最终故障集合以及当前充放电权限。
 */
typedef struct
{
    /**
     * @brief 当前实时有效故障位
     *
     * 表示当前保护条件仍处于异常状态。
     *
     * 当恢复条件连续满足 recoverMs 后，
     * 对应位可被自动清除。
     */
    uint32_t activeFaults;

    /**
     * @brief 已锁存故障位
     *
     * 对配置为锁存的故障，
     * 即使 activeFaults 已恢复，
     * 仍会保留在此字段中。
     *
     * 需要调用 bms_protect_ClearLatched() 显式清除。
     */
    uint32_t latchedFaults;

    /**
     * @brief 最终对外故障集合
     *
     * faults = activeFaults | latchedFaults
     */
    uint32_t faults;

    /**
     * @brief 当前是否允许 VIN 输入/充电通路
     *
     * true：
     * 当前不存在 BMS_PROTECT_CHARGE_BLOCK_MASK 中的故障。
     *
     * false：
     * 当前软件保护禁止充电输入通路。
     */
    bool chargeAllowed;

    /**
     * @brief 当前是否允许 VBAT 放电/电池输出通路
     *
     * true：
     * 当前不存在 BMS_PROTECT_DISCHARGE_BLOCK_MASK 中的故障。
     *
     * false：
     * 当前软件保护禁止电池放电通路。
     */
    bool dischargeAllowed;

} bms_protect_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_protect_Init
//  Description: 初始化 BMS 软件保护模块
//  Param      : config - 软件保护配置；传入 NULL 时使用 bms_config.h 默认配置
//  Return     : true：初始化成功；false：配置参数非法
//  Others     : 配置非法时保持禁止充放电状态；
//               启用模拟量保护时，在首份有效采样到来前置位 SAMPLE_INVALID
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_protect_Init(const bms_protect_config_t *config);


/*******************************************************************************
//  Function   : bms_protect_UpdateSample
//  Description: 更新 BMS 软件保护采样快照
//  Param      : sample - 最新一组完整采样数据
//  Return     : None
//  Others     : 建议在线程或主循环中调用；ADC ISR 应仅保存原始数据或提交完整快照，
//               不建议直接在中断中执行保护判定
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_UpdateSample(const bms_protect_sample_t *sample);


/*******************************************************************************
//  Function   : bms_protect_1ms_task
//  Description: BMS 软件保护 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成采样新鲜度检查、各项模拟量保护触发/恢复、
//               外部故障处理以及 chargeAllowed / dischargeAllowed 更新
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_1ms_task(void);


/*******************************************************************************
//  Function   : bms_protect_SetExternalFault
//  Description: 设置或清除上层外部故障
//  Param      : fault - true：设置外部故障；false：清除外部故障
//  Return     : None
//  Others     : 外部故障不经过 confirmMs / recoverMs 延时，立即参与充放电联锁；
//               当前 IP3108A 未向 MCU 提供直接故障输出，因此该接口由上层显式调用
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_SetExternalFault(bool fault);


/*******************************************************************************
//  Function   : bms_protect_ClearLatched
//  Description: 清除指定的已锁存故障
//  Param      : mask - 请求清除的故障位掩码
//  Return     : None
//  Others     : 仅允许清除当前已经恢复的锁存故障；
//               仍存在于 activeFaults 中的故障不会被清除；
//               SAMPLE_INVALID 存在时不允许清除模拟量锁存故障
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_ClearLatched(uint32_t mask);


/*******************************************************************************
//  Function   : bms_protect_GetStatus
//  Description: 获取当前 BMS 软件保护状态
//  Param      : None
//  Return     : 指向内部 bms_protect_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改其内容
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_protect_status_t *bms_protect_GetStatus(void);


#endif /* _BMS_PROTECT_H_ */
