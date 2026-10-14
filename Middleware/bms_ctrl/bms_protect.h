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
 *          3. ADC ISR 不直接执行保护逻辑，由主循环或周期任务提交完整采样数据。
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
 */
typedef struct
{
    uint32_t enabledMask;              /**< 启用的模拟量保护位，0 表示关闭所有模拟量自动保护 */
    uint32_t latchMask;                /**< 故障锁存位，锁存故障需通过 bms_protect_ClearLatched() 清除 */

    uint16_t confirmMs;                /**< 故障连续触发确认时间，单位 ms */
    uint16_t recoverMs;                /**< 故障连续恢复确认时间，单位 ms */
    uint16_t sampleTimeoutMs;          /**< 采样数据最大允许年龄，单位 ms */

    uint16_t batteryLowMv;             /**< 电池欠压触发阈值，单位 mV */
    uint16_t batteryLowRecoverMv;      /**< 电池欠压恢复阈值，必须大于 batteryLowMv */

    uint16_t batteryHighMv;            /**< 电池过压触发阈值，单位 mV */
    uint16_t batteryHighRecoverMv;     /**< 电池过压恢复阈值，必须小于 batteryHighMv */

    uint16_t usbHighMv;                /**< USB 输入过压触发阈值，单位 mV */
    uint16_t usbHighRecoverMv;         /**< USB 输入过压恢复阈值，必须小于 usbHighMv */

    int16_t temperatureHighDeciC;      /**< 过温触发阈值，单位 0.1°C */
    int16_t temperatureRecoverDeciC;   /**< 过温恢复阈值，必须小于 temperatureHighDeciC */

    uint16_t currentHighMa;            /**< 负载过流触发阈值，单位 mA */
    uint16_t currentRecoverMa;         /**< 负载过流恢复阈值，必须小于 currentHighMa */

} bms_protect_config_t;


/**
 * @brief BMS 软件保护采样数据
 */
typedef struct
{
    uint32_t validMask;                /**< 当前有效采样位，转换失败时清除对应有效位 */

    uint16_t batteryMv;                /**< 当前电池电压，单位 mV，由上层提供 */
    uint16_t usbMv;                    /**< 当前 USB 输入电压，单位 mV */
    int16_t temperatureDeciC;          /**< 当前温度，单位 0.1°C */
    uint16_t currentMa;                /**< 当前负载电流，单位 mA */

    bool usbPresent;                   /**< USB 去抖后的插入状态，用于 USB 过压判断 */
    bool loadEnabled;                  /**< 当前负载启用状态，用于过流判断 */

} bms_protect_sample_t;


/**
 * @brief BMS 软件保护状态
 */
typedef struct
{
    uint32_t activeFaults;             /**< 当前实时有效故障 */
    uint32_t latchedFaults;            /**< 已锁存故障，需显式清除 */
    uint32_t faults;                   /**< 最终故障集合：activeFaults | latchedFaults */

    bool chargeAllowed;                /**< 是否允许 VIN 输入/充电通路 */
    bool dischargeAllowed;             /**< 是否允许 VBAT 放电/电池通路 */

} bms_protect_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

#if BMS_MODULE_PROTECT_ENABLE

/*******************************************************************************
//  Function   : bms_protect_Init
//  Description: 初始化 BMS 软件保护模块
//  Param      : config - 软件保护配置；传入 NULL 时使用 bms_config.h 默认配置
//  Return     : true：初始化成功；false：配置参数非法
//  Others     : 启用模拟量保护时，首份有效采样到来前置位 SAMPLE_INVALID
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_protect_Init(const bms_protect_config_t *config);


/*******************************************************************************
//  Function   : bms_protect_UpdateSample
//  Description: 更新 BMS 软件保护采样数据
//  Param      : sample - 最新一组完整采样数据
//  Return     : None
//  Others     : 建议在线程或主循环中调用，不建议直接在 ADC ISR 中执行保护逻辑
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_UpdateSample(const bms_protect_sample_t *sample);


/*******************************************************************************
//  Function   : bms_protect_1ms_task
//  Description: BMS 软件保护 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成采样有效性检查、故障触发/恢复及充放电权限更新
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_1ms_task(void);


/*******************************************************************************
//  Function   : bms_protect_SetExternalFault
//  Description: 设置或清除上层外部故障
//  Param      : fault - true：设置故障；false：清除故障
//  Return     : None
//  Others     : 外部故障立即参与充放电联锁，不经过 confirmMs / recoverMs 延时
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_SetExternalFault(bool fault);


/*******************************************************************************
//  Function   : bms_protect_ClearLatched
//  Description: 清除指定的已锁存故障
//  Param      : mask - 请求清除的故障位掩码
//  Return     : None
//  Others     : 仅清除已经恢复的锁存故障；当前仍有效的故障不会被清除
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_ClearLatched(uint32_t mask);


/*******************************************************************************
//  Function   : bms_protect_GetStatus
//  Description: 获取当前 BMS 软件保护状态
//  Param      : None
//  Return     : 指向内部 bms_protect_status_t 的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_protect_status_t *bms_protect_GetStatus(void);


#endif /* BMS_MODULE_PROTECT_ENABLE */
#endif /* _BMS_PROTECT_H_ */
