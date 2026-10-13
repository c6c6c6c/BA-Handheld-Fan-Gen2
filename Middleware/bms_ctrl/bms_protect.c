/**
 ***********************************************************************************************************************
 * @file    bms_protect.c
 * @brief   BMS 软件保护模块，实现电压、温度、电流、采样有效性及外部故障的软件联锁保护
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @note
 *          1. 本模块属于软件保护层，不替代 USB 板 IP3108A 等硬件电池保护。
 *          2. 支持故障触发延时、恢复延时、恢复滞回、故障锁存及采样超时检测。
 *          3. chargeAllowed / dischargeAllowed 由当前故障集合与配置的联锁掩码共同决定。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_protect.h"
#include <string.h>


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 当前软件保护配置
 *
 * 初始化时由调用方传入，
 * 如果 bms_protect_Init(NULL)，则使用 BMS_PROTECT_DEFAULT_CONFIG。
 */
static bms_protect_config_t s_config;


/**
 * @brief 当前最新一组 BMS 采样数据
 *
 * 由 bms_protect_UpdateSample() 更新。
 */
static bms_protect_sample_t s_sample;


/**
 * @brief 当前软件保护状态
 *
 * 包含：
 *
 * - activeFaults      当前实时有效故障
 * - latchedFaults     已锁存故障
 * - faults            activeFaults 与 latchedFaults 的并集
 * - chargeAllowed     当前是否允许充电
 * - dischargeAllowed  当前是否允许放电
 */
static bms_protect_status_t s_status;


/**
 * @brief 每种模拟量保护的连续触发计时
 *
 * 数组下标与 BMS_PROTECT_xxx_INDEX 对应。
 *
 * 当触发条件持续满足时每 1ms 累加，
 * 达到 confirmMs 后正式置位故障。
 */
static uint16_t s_confirm[BMS_PROTECT_ANALOG_COUNT];


/**
 * @brief 每种模拟量保护的连续恢复计时
 *
 * 数组下标与 BMS_PROTECT_xxx_INDEX 对应。
 *
 * 当恢复条件持续满足时每 1ms 累加，
 * 达到 recoverMs 后清除 activeFaults 中对应故障位。
 */
static uint16_t s_recover[BMS_PROTECT_ANALOG_COUNT];


/**
 * @brief 当前采样数据年龄
 *
 * 单位：ms
 *
 * 每次 bms_protect_UpdateSample() 后清零，
 * bms_protect_1ms_task() 中每 1ms 累加。
 */
static uint16_t s_sampleAge;


/**
 * @brief 软件保护模块初始化完成标志
 */
static bool s_initialized;


/**
 * @brief 是否至少接收到过一组采样数据
 */
static bool s_hasSample;


/**
 * @brief 上层显式设置的外部故障状态
 */
static bool s_externalFault;


/*******************************************************************************
//  Function   : bms_protect_RefreshStatus
//  Description: 刷新故障集合及充放电允许状态
//  Param      : None
//  Return     : None
//  Others     : 根据 activeFaults、latchedFaults 及联锁掩码计算最终保护状态
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static void bms_protect_RefreshStatus(void)
{
    /*
     * 将当前有效故障中配置为锁存的故障加入 latchedFaults。
     *
     * latchedFaults 一旦置位，
     * 即使 activeFaults 后续恢复，也不会自动清除。
     */
    s_status.latchedFaults |= s_status.activeFaults & s_config.latchMask;

    /*
     * 对外故障集合：
     *
     * faults = 当前实时故障 | 已锁存故障
     */
    s_status.faults = s_status.activeFaults | s_status.latchedFaults;

    /*
     * 允许充电条件：
     *
     * 1. 软件保护模块已经初始化
     * 2. 当前 faults 中不存在任何禁止充电的故障
     */
    s_status.chargeAllowed = s_initialized && ((s_status.faults & BMS_PROTECT_CHARGE_BLOCK_MASK) == 0U);

    /*
     * 允许放电条件：
     *
     * 1. 软件保护模块已经初始化
     * 2. 当前 faults 中不存在任何禁止放电的故障
     */
    s_status.dischargeAllowed = s_initialized && ((s_status.faults & BMS_PROTECT_DISCHARGE_BLOCK_MASK) == 0U);
}


/*******************************************************************************
//  Function   : bms_protect_ConfigValid
//  Description: 检查软件保护配置参数是否合法
//  Param      : c - 待检查的保护配置
//  Return     : true：配置合法；false：配置非法
//  Others     : 默认配置和用户传入配置均执行相同检查，防止启用未配置的占位阈值
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static bool bms_protect_ConfigValid(const bms_protect_config_t *c)
{
    /*
     * enabledMask 只能包含模拟量保护位。
     *
     * latchMask 可以包含当前模块支持的全部故障位。
     */
    if (((c->enabledMask & ~BMS_PROTECT_ANALOG_MASK) != 0U) 
    ||  ((c->latchMask & ~BMS_PROTECT_ALL_MASK) != 0U))
    {
        return false;
    }

    /*
     * 只要启用了任意模拟量保护，
     * 以下三个时间参数必须配置为非 0：
     *
     * confirmMs       故障触发确认时间
     * recoverMs       故障恢复确认时间
     * sampleTimeoutMs 采样最大允许年龄
     */
    if ((c->enabledMask != 0U) 
    && ((c->confirmMs == 0U) ||(c->recoverMs == 0U) || (c->sampleTimeoutMs == 0U)))
    {
        return false;
    }

    /*
     * 电池欠压保护：
     *
     * 欠压触发值必须非 0，
     * 恢复电压必须高于触发电压，形成恢复滞回。
     */
    if (((c->enabledMask & BMS_PROTECT_BATTERY_LOW) != 0U) 
    &&  ((c->batteryLowMv == 0U) || (c->batteryLowRecoverMv <= c->batteryLowMv)))
    {
        return false;
    }

    /*
     * 电池过压保护：
     *
     * 恢复电压必须低于过压触发电压。
     */
    if (((c->enabledMask & BMS_PROTECT_BATTERY_HIGH) != 0U) && (c->batteryHighRecoverMv >= c->batteryHighMv))
    {
        return false;
    }

    /*
     * 同时启用电池欠压和过压保护时，
     * 欠压恢复点必须低于过压恢复点，
     * 防止有效工作电压区间发生重叠或颠倒。
     */
    if (((c->enabledMask & (BMS_PROTECT_BATTERY_LOW | BMS_PROTECT_BATTERY_HIGH)) == (BMS_PROTECT_BATTERY_LOW | BMS_PROTECT_BATTERY_HIGH))
     && (c->batteryLowRecoverMv >= c->batteryHighRecoverMv))
    {
        return false;
    }

    /*
     * USB 过压保护：
     *
     * 1. 恢复电压必须低于触发电压
     * 2. USB_HIGH 当前设计要求必须配置为锁存故障
     */
    if (((c->enabledMask & BMS_PROTECT_USB_HIGH) != 0U) 
    &&  ((c->usbHighRecoverMv >= c->usbHighMv) || ((c->latchMask & BMS_PROTECT_USB_HIGH) == 0U)))
    {
        return false;
    }

    /*
     * 过温保护：
     *
     * 恢复温度必须低于过温触发温度。
     */
    if (((c->enabledMask & BMS_PROTECT_TEMP_HIGH) != 0U) && (c->temperatureRecoverDeciC >= c->temperatureHighDeciC))
    {
        return false;
    }

    /*
     * 过流保护：
     *
     * 恢复电流必须低于过流触发电流。
     */
    if (((c->enabledMask & BMS_PROTECT_CURRENT_HIGH) != 0U) && (c->currentRecoverMa >= c->currentHighMa))
    {
        return false;
    }

    return true;
}


/*******************************************************************************
//  Function   : bms_protect_Init
//  Description: 初始化 BMS 软件保护模块
//  Param      : config - 软件保护配置；传入 NULL 时使用默认配置
//  Return     : true：初始化成功；false：保护参数配置非法
//  Others     : 启用模拟量保护时，在收到首份有效采样前置位 SAMPLE_INVALID，
//               从而禁止相关充放电通路
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_protect_Init(const bms_protect_config_t *config)
{
    /*
     * 默认配置由 bms_config.h 中的宏生成。
     */
    static const bms_protect_config_t defaultConfig = BMS_PROTECT_DEFAULT_CONFIG;

    /*
     * 用户传入配置优先，
     * NULL 时使用默认配置。
     */
    const bms_protect_config_t *selectedConfig = (config != 0) ? config : &defaultConfig;


    /*
     * 清零软件保护内部状态。
     */
    memset(&s_config, 0, sizeof(s_config));
    memset(&s_sample, 0, sizeof(s_sample));
    memset(&s_status, 0, sizeof(s_status));
    memset(s_confirm, 0, sizeof(s_confirm));
    memset(s_recover, 0, sizeof(s_recover));

    s_initialized   = false;
    s_hasSample     = false;
    s_externalFault = false;
    s_sampleAge     = 0U;


    /*
     * 参数配置非法时初始化失败。
     *
     * 同时置位 SAMPLE_INVALID，
     * 确保上层不会误认为当前允许充放电。
     */
    if (!bms_protect_ConfigValid(selectedConfig))
    {
        s_status.activeFaults = BMS_PROTECT_SAMPLE_INVALID;
        bms_protect_RefreshStatus();

        return false;
    }


    /*
     * 保存有效保护配置。
     */
    s_config = *selectedConfig;

    s_initialized = true;


    /*
     * 如果启用了模拟量保护：
     *
     * 首份有效采样到来前，
     * 无法确认电压、温度、电流是否安全，
     * 因此先置位 SAMPLE_INVALID。
     */
    if (s_config.enabledMask != 0U)
    {
        s_status.activeFaults = BMS_PROTECT_SAMPLE_INVALID;
        bms_protect_RefreshStatus();
    }
    else
    {
        /*
         * 未启用任何模拟量保护时，
         * 不要求必须存在采样数据。
         */
        bms_protect_RefreshStatus();
    }

    return true;
}


/*******************************************************************************
//  Function   : bms_protect_UpdateSample
//  Description: 更新 BMS 软件保护采样数据
//  Param      : sample - 最新一组保护采样数据
//  Return     : None
//  Others     : 更新成功后采样年龄清零，并标记已经收到有效采样
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_UpdateSample(const bms_protect_sample_t *sample)
{
    if (s_initialized && (sample != 0))
    {
        /*
         * 保存最新一组采样。
         */
        s_sample = *sample;

        /*
         * 新数据到来后重新开始计算数据年龄。
         */
        s_sampleAge = 0U;

        /*
         * 标记至少已经收到过一组采样。
         */
        s_hasSample = true;
    }
}


/*******************************************************************************
//  Function   : bms_protect_Check
//  Description: 执行单项模拟量保护的触发及恢复判定
//  Param      : index   - 对应保护项索引
//               valid   - 当前采样数据是否有效
//               trip    - 当前是否满足故障触发条件
//               recover - 当前是否满足故障恢复条件
//  Return     : None
//  Others     : 支持连续触发确认、连续恢复确认及恢复滞回；
//               采样无效时中断连续计时，但不会直接清除已存在故障
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static void bms_protect_Check(uint8_t index,
                              bool valid,
                              bool trip,
                              bool recover)
{
    uint32_t bit = 1UL << index;


    /*
     * 当前保护项未启用时直接返回。
     */
    if ((s_config.enabledMask & bit) == 0U)
    {
        return;
    }


    /*
     * 当前测量值无效时：
     *
     * - 清除连续触发计数
     * - 清除连续恢复计数
     * - 不修改当前故障状态
     *
     * SAMPLE_INVALID 由外层统一处理。
     */
    if (!valid)
    {
        s_confirm[index] = 0U;
        s_recover[index] = 0U;
        return;
    }


    /*
     * 当前该故障尚未触发。
     */
    if ((s_status.activeFaults & bit) == 0U)
    {
        /*
         * 未处于故障状态时，不需要累计恢复时间。
         */
        s_recover[index] = 0U;

        if (trip)
        {
            /*
             * 故障条件连续满足，
             * 累加触发确认时间。
             */
            if (++s_confirm[index] >= s_config.confirmMs)
            {
                /*
                 * 达到确认时间后正式置位故障。
                 */
                s_status.activeFaults |= bit;

                s_confirm[index] = 0U;
            }
        }
        else
        {
            /*
             * 触发条件中途消失，
             * 连续确认时间重新计算。
             */
            s_confirm[index] = 0U;
        }
    }
    else
    {
        /*
         * 当前故障已经处于激活状态。
         */
        s_confirm[index] = 0U;

        if (recover)
        {
            /*
             * 恢复条件连续满足，
             * 累加恢复确认时间。
             */
            if (++s_recover[index] >= s_config.recoverMs)
            {
                /*
                 * 达到恢复确认时间后，
                 * 清除 activeFaults 中的故障位。
                 *
                 * 如果该故障已经锁存，
                 * latchedFaults 仍会保持。
                 */
                s_status.activeFaults &= ~bit;

                s_recover[index] = 0U;
            }
        }
        else
        {
            /*
             * 恢复条件中途消失，
             * 连续恢复时间重新计算。
             */
            s_recover[index] = 0U;
        }
    }
}


/*******************************************************************************
//  Function   : bms_protect_1ms_task
//  Description: BMS 软件保护 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成采样超时判断、有效位检查、各项模拟量保护、外部故障处理及
//               chargeAllowed / dischargeAllowed 状态更新
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_1ms_task(void)
{
    uint32_t required = 0U;
    uint32_t valid;

    bool fresh;
    bool batteryValid;
    bool usbValid;
    bool temperatureValid;
    bool currentValid;


    if (!s_initialized)
    {
        return;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Sample freshness                                                                                                */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 当前采样有效条件：
     *
     * 1. 至少收到过一组采样
     * 2. 当前数据年龄小于 sampleTimeoutMs
     */
    fresh = s_hasSample && (s_sampleAge < s_config.sampleTimeoutMs);


    /*
     * 数据年龄达到 timeout 后停止继续累加，
     * 防止 uint16_t 回绕后旧采样重新被判断为新数据。
     */
    if (s_sampleAge < s_config.sampleTimeoutMs)
    {
        s_sampleAge++;
    }


    /*
     * 数据未超时时使用采样中的 validMask，
     * 超时时认为所有采样均无效。
     */
    valid = fresh ? s_sample.validMask : 0U;


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Required sample mask                                                                                            */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 电池欠压 / 过压保护启用时，
     * 必须提供有效 batteryMv。
     */
    if ((s_config.enabledMask & (BMS_PROTECT_BATTERY_LOW | BMS_PROTECT_BATTERY_HIGH)) != 0U)
    {
        required |= BMS_SAMPLE_BATTERY;
    }


    /*
     * USB 过压保护：
     *
     * USB 存在时要求 USB 电压采样有效。
     *
     * 如果 USB_HIGH 已经触发，
     * 即使 USB 后续被拔出，
     * 仍要求获得有效 USB 采样后才能执行恢复判断。
     */
    if (((s_config.enabledMask & BMS_PROTECT_USB_HIGH) != 0U) 
    &&  (s_sample.usbPresent || ((s_status.activeFaults & BMS_PROTECT_USB_HIGH) != 0U)))
    {
        required |= BMS_SAMPLE_USB;
    }


    /*
     * 启用过温保护时，
     * 必须提供有效温度采样。
     */
    if ((s_config.enabledMask & BMS_PROTECT_TEMP_HIGH) != 0U)
    {
        required |= BMS_SAMPLE_TEMPERATURE;
    }


    /*
     * 过流保护：
     *
     * 负载开启时要求电流采样有效。
     *
     * 如果过流已经触发，
     * 即使负载关闭，
     * 仍要求有效采样用于确认恢复。
     */
    if (((s_config.enabledMask & BMS_PROTECT_CURRENT_HIGH) != 0U) 
    &&  (s_sample.loadEnabled || ((s_status.activeFaults & BMS_PROTECT_CURRENT_HIGH) != 0U)))
    {
        required |= BMS_SAMPLE_CURRENT;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Sample invalid protection                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 满足以下任意条件时置位 SAMPLE_INVALID：
     *
     * 1. 已启用模拟量保护但当前数据超时
     * 2. required 中要求的采样项没有全部有效
     */
    if ((s_config.enabledMask != 0U) && (!fresh || ((valid & required) != required)))
    {
        s_status.activeFaults |= BMS_PROTECT_SAMPLE_INVALID;
    }
    else
    {
        s_status.activeFaults &= ~BMS_PROTECT_SAMPLE_INVALID;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Individual sample valid state                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

    batteryValid     = (valid & BMS_SAMPLE_BATTERY)     != 0U;
    usbValid         = (valid & BMS_SAMPLE_USB)         != 0U;
    temperatureValid = (valid & BMS_SAMPLE_TEMPERATURE) != 0U;
    currentValid     = (valid & BMS_SAMPLE_CURRENT)     != 0U;


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Battery undervoltage                                                                                            */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 触发：
     * batteryMv <= batteryLowMv
     *
     * 恢复：
     * batteryMv >= batteryLowRecoverMv
     */
    bms_protect_Check(BMS_PROTECT_BATTERY_LOW_INDEX,
                      batteryValid,
                      s_sample.batteryMv <= s_config.batteryLowMv,
                      s_sample.batteryMv >= s_config.batteryLowRecoverMv);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Battery overvoltage                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 触发：
     * batteryMv >= batteryHighMv
     *
     * 恢复：
     * batteryMv <= batteryHighRecoverMv
     */
    bms_protect_Check(BMS_PROTECT_BATTERY_HIGH_INDEX,
                      batteryValid,
                      s_sample.batteryMv >= s_config.batteryHighMv,
                      s_sample.batteryMv <= s_config.batteryHighRecoverMv);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* USB overvoltage                                                                                                 */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 触发：
     *
     * USB 已插入
     * &&
     * usbMv >= usbHighMv
     *
     * 恢复：
     *
     * usbMv <= usbHighRecoverMv
     */
    bms_protect_Check(BMS_PROTECT_USB_HIGH_INDEX,
                      usbValid,
                      s_sample.usbPresent &&
                      (s_sample.usbMv >= s_config.usbHighMv),
                      s_sample.usbMv  <= s_config.usbHighRecoverMv);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Over temperature                                                                                                */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 触发：
     * temperatureDeciC >= temperatureHighDeciC
     *
     * 恢复：
     * temperatureDeciC <= temperatureRecoverDeciC
     */
    bms_protect_Check(BMS_PROTECT_TEMP_HIGH_INDEX,
                      temperatureValid,
                      s_sample.temperatureDeciC >= s_config.temperatureHighDeciC,
                      s_sample.temperatureDeciC <= s_config.temperatureRecoverDeciC);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Over current                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 触发：
     *
     * loadEnabled == true
     * &&
     * currentMa >= currentHighMa
     *
     * 恢复：
     *
     * currentMa <= currentRecoverMa
     */
    bms_protect_Check(BMS_PROTECT_CURRENT_HIGH_INDEX,
                      currentValid,
                      s_sample.loadEnabled && (s_sample.currentMa  >= s_config.currentHighMa),
                      s_sample.currentMa   <= s_config.currentRecoverMa);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* External fault                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 上层主动报告的外部故障。
     */
    if (s_externalFault)
    {
        s_status.activeFaults |= BMS_PROTECT_EXTERNAL;
    }
    else
    {
        s_status.activeFaults &= ~BMS_PROTECT_EXTERNAL;
    }


    /*
     * 根据最新故障状态重新计算：
     *
     * - latchedFaults
     * - faults
     * - chargeAllowed
     * - dischargeAllowed
     */
    bms_protect_RefreshStatus();
}


/*******************************************************************************
//  Function   : bms_protect_SetExternalFault
//  Description: 设置或清除上层外部故障
//  Param      : fault - true：设置外部故障；false：清除外部故障
//  Return     : None
//  Others     : 外部故障立即参与充放电联锁，不经过 confirmMs / recoverMs 延时
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_SetExternalFault(bool fault)
{
    /*
     * 保存外部故障请求状态。
     */
    s_externalFault = fault;

    if (fault)
    {
        s_status.activeFaults |= BMS_PROTECT_EXTERNAL;
    }
    else
    {
        s_status.activeFaults &= ~BMS_PROTECT_EXTERNAL;
    }

    /*
     * 立即刷新充放电权限。
     */
    bms_protect_RefreshStatus();
}


/*******************************************************************************
//  Function   : bms_protect_ClearLatched
//  Description: 清除指定的已锁存故障
//  Param      : mask - 请求清除的故障位掩码
//  Return     : None
//  Others     : 当前仍处于 activeFaults 的故障不能被清除；
//               SAMPLE_INVALID 存在时，不允许清除模拟量锁存故障
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_protect_ClearLatched(uint32_t mask)
{
    /*
     * 当前采样数据无效时，
     * 无法确认电压、温度、电流等模拟量已经真正恢复正常。
     *
     * 因此此时禁止清除模拟量保护对应的锁存故障。
     */
    if ((s_status.activeFaults &
         BMS_PROTECT_SAMPLE_INVALID) != 0U)
    {
        mask &= ~BMS_PROTECT_ANALOG_MASK;
    }

    /*
     * 只允许清除当前已经不再 active 的锁存故障。
     *
     * 如果某故障仍然处于 activeFaults，
     * 即使上层请求清除也会保留。
     */
    s_status.latchedFaults &=
        ~(mask & ~s_status.activeFaults);

    /*
     * 清除锁存故障后重新计算最终故障集合及充放电权限。
     */
    bms_protect_RefreshStatus();
}


/*******************************************************************************
//  Function   : bms_protect_GetStatus
//  Description: 获取当前 BMS 软件保护状态
//  Param      : None
//  Return     : 指向内部 bms_protect_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改其内容
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_protect_status_t *bms_protect_GetStatus(void)
{
    return &s_status;
}
