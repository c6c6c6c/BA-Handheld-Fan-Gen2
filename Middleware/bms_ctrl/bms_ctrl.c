/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.c
 * @brief   BMS 控制模块，实现 VIN/VBAT 通路控制、USB 插入检测及 IP5326 KEY 非阻塞按键控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_ctrl.h"
#include <string.h>


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS KEY 控制状态机阶段
 *
 * 用于通过 1ms 周期任务实现 IP5326 KEY 的短按、长按及双击操作，
 * 避免使用阻塞式延时。
 */
typedef enum
{
    BMS_KEY_IDLE = 0,          /**< 空闲状态，无 KEY 操作 */
    BMS_KEY_FIRST_PRESS,       /**< 第一次按键保持阶段 */
    BMS_KEY_DOUBLE_GAP,        /**< 双击操作两次按键之间的释放间隔 */
    BMS_KEY_SECOND_PRESS,      /**< 双击操作第二次按键保持阶段 */
    BMS_KEY_GUARD              /**< 按键释放保护阶段，防止连续命令过快执行 */
} bms_key_phase_t;


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 控制模块当前运行状态
 */
static bms_ctrl_status_t s_status;

/**
 * @brief BMS 控制模块初始化完成标志
 */
static bool s_initialized;

/**
 * @brief VIN 输入通路软件请求状态
 *
 * 最终 VIN_EN 输出还需要与电池保护模块的 chargeAllowed 状态共同决定。
 */
static bool s_vinRequested;

/**
 * @brief VBAT 电池通路软件请求状态
 *
 * 最终 VBAT_EN 输出还需要同时满足非待机状态以及 dischargeAllowed 条件。
 */
static bool s_vbatRequested;

/**
 * @brief USB 输入检测当前候选状态
 *
 * 用于 USB 插拔去抖。
 */
static bool s_usbCandidate;

/**
 * @brief USB 当前候选状态持续时间，单位 ms
 */
static uint16_t s_usbStableMs;

/**
 * @brief 当前 KEY 状态机阶段剩余时间，单位 ms
 */
static uint16_t s_keyRemainingMs;

/**
 * @brief 当前 KEY 状态机阶段
 */
static bms_key_phase_t s_keyPhase;

/**
 * @brief 当前正在执行的 KEY 控制命令
 */
static bms_key_command_t s_keyCommand;


/*******************************************************************************
//  Function   : bms_ctrl_FaultChangedCallback
//  Description: BMS 故障状态变化回调函数
//  Param      : faults - 当前 BMS 故障标志集合
//  Return     : None
//  Others     : Weak 函数，应用层可重新实现，用于故障上报或状态处理
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
__WEAK void bms_ctrl_FaultChangedCallback(uint32_t faults)
{
    (void)faults;
}


/*******************************************************************************
//  Function   : bms_ctrl_SetKey
//  Description: 设置 IP5326 KEY 按键状态
//  Param      : pressed - true：模拟按键按下；false：模拟按键释放
//  Return     : None
//  Others     : KEY 实际 GPIO 极性由 bms_config.h 中 BMS_IO_KEY_xxx 宏适配
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static void bms_ctrl_SetKey(bool pressed)
{
    if (pressed)
    {
        BMS_IO_KEY_PRESS();
    }
    else
    {
        BMS_IO_KEY_RELEASE();
    }
}


/*******************************************************************************
//  Function   : bms_ctrl_ApplyOutputs
//  Description: 更新 VIN_EN 和 VBAT_EN 实际硬件输出状态
//  Param      : None
//  Return     : None
//  Others     : 最终输出由软件请求、模块初始化状态、待机状态以及保护模块权限共同决定；
//               s_status.vinEnabled / vbatEnabled 表示当前输出命令状态，不代表硬件真实反馈
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static void bms_ctrl_ApplyOutputs(void)
{
    const bms_protect_status_t *protect = bms_protect_GetStatus();

    /*
     * VIN 输入通路允许条件：
     *
     * 1. BMS 控制模块已初始化
     * 2. 软件请求开启 VIN
     * 3. 电池保护模块允许充电
     */
    bool vin = s_initialized &&
               s_vinRequested &&
               protect->chargeAllowed;

    /*
     * VBAT 电池通路允许条件：
     *
     * 1. BMS 控制模块已初始化
     * 2. 软件请求开启 VBAT
     * 3. 当前未请求进入待机
     * 4. 电池保护模块允许放电
     */
    bool vbat = s_initialized &&
                s_vbatRequested &&
                !s_status.standbyRequested &&
                protect->dischargeAllowed;

    if (vin)
    {
        BMS_IO_VIN_ENABLE();
    }
    else
    {
        BMS_IO_VIN_DISABLE();
    }

    if (vbat)
    {
        BMS_IO_VBAT_ENABLE();
    }
    else
    {
        BMS_IO_VBAT_DISABLE();
    }

    /*
     * 记录当前软件输出状态。
     */
    s_status.vinEnabled  = vin;
    s_status.vbatEnabled = vbat;
}


/*******************************************************************************
//  Function   : bms_ctrl_Init
//  Description: 初始化 BMS 控制模块
//  Param      : protectConfig - BMS 保护参数配置
//  Return     : true：初始化成功；false：初始化失败
//  Others     : 初始化 VIN/VBAT 请求状态、USB检测、KEY状态机及电池保护模块；
//               GPIO 未正确预配置或保护参数无效时初始化失败
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_Init(const bms_protect_config_t *protectConfig)
{
    bool valid;

    /*
     * 清零 BMS 控制状态。
     */
    memset(&s_status, 0, sizeof(s_status));

    s_initialized   = false;
    s_vinRequested  = BMS_CTRL_INIT_VIN_ENABLED;
    s_vbatRequested = BMS_CTRL_INIT_VBAT_ENABLED;

    /*
     * 初始化 USB 插入检测去抖状态。
     */
    s_usbCandidate = false;
    s_usbStableMs  = 0U;

    /*
     * 初始化 KEY 状态机。
     */
    s_keyRemainingMs = 0U;
    s_keyPhase       = BMS_KEY_IDLE;
    s_keyCommand     = BMS_KEY_WAKEUP;

    /*
     * 检查底层 GPIO 是否已经正确配置。
     *
     * BMS 模块不主动重新配置可能与 PWM、显示等功能复用的 GPIO，
     * 如果底层 IO 状态不满足要求，则直接初始化失败。
     */
    valid = BMS_IO_IS_READY();

    if (!valid)
    {
        return false;
    }

    /*
     * 默认释放 IP5326 KEY。
     */
    bms_ctrl_SetKey(false);

    /*
     * 初始化电池保护模块。
     */
    s_initialized = bms_protect_Init(protectConfig);

    /*
     * 同步初始保护故障状态。
     */
    s_status.faults = bms_protect_GetStatus()->faults;

    /*
     * 根据初始化配置及保护状态设置 VIN/VBAT 输出。
     */
    bms_ctrl_ApplyOutputs();

    return s_initialized;
}


/*******************************************************************************
//  Function   : bms_ctrl_KeyTask
//  Description: BMS KEY 非阻塞状态机 1ms 处理任务
//  Param      : None
//  Return     : None
//  Others     : 实现短按、长按、双击以及按键释放保护阶段
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
static void bms_ctrl_KeyTask(void)
{
    if (s_keyPhase == BMS_KEY_IDLE)
    {
        return;
    }

    /*
     * 当前阶段计时。
     */
    if (s_keyRemainingMs != 0U)
    {
        s_keyRemainingMs--;
    }

    /*
     * 当前阶段时间尚未结束。
     */
    if (s_keyRemainingMs != 0U)
    {
        return;
    }

    /*
     * 当前阶段时间结束后推进 KEY 状态机。
     */
    switch (s_keyPhase)
    {
        case BMS_KEY_FIRST_PRESS:

            /*
             * 第一次按键完成，释放 KEY。
             */
            bms_ctrl_SetKey(false);

            if (s_keyCommand == BMS_KEY_LIGHT_TOGGLE)
            {
                /*
                 * 照明灯切换需要双击：
                 * 第一次短按结束后进入双击间隔。
                 */
                s_keyPhase = BMS_KEY_DOUBLE_GAP;
                s_keyRemainingMs = BMS_CTRL_KEY_DOUBLE_GAP_MS;
            }
            else
            {
                /*
                 * 普通短按或长按结束后进入释放保护阶段。
                 */
                s_keyPhase = BMS_KEY_GUARD;
                s_keyRemainingMs = BMS_CTRL_KEY_GUARD_MS;
            }

            break;

        case BMS_KEY_DOUBLE_GAP:

            /*
             * 双击间隔结束，开始第二次短按。
             */
            bms_ctrl_SetKey(true);

            s_keyPhase = BMS_KEY_SECOND_PRESS;
            s_keyRemainingMs = BMS_CTRL_KEY_SHORT_MS;

            break;

        case BMS_KEY_SECOND_PRESS:

            /*
             * 第二次短按结束，释放 KEY。
             */
            bms_ctrl_SetKey(false);

            s_keyPhase = BMS_KEY_GUARD;
            s_keyRemainingMs = BMS_CTRL_KEY_GUARD_MS;

            break;

        default:

            /*
             * GUARD 阶段结束或异常状态恢复：
             * 强制释放 KEY 并回到空闲状态。
             */
            bms_ctrl_SetKey(false);
            s_keyPhase = BMS_KEY_IDLE;

            break;
    }

    /*
     * 更新 KEY 忙状态。
     */
    s_status.keyBusy = (s_keyPhase != BMS_KEY_IDLE);
}


/*******************************************************************************
//  Function   : bms_ctrl_1ms_task
//  Description: BMS 控制模块 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成 USB 插入去抖、LED 状态采集、电池保护处理、KEY 状态机、
//               VIN/VBAT 输出更新及故障变化通知
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_1ms_task(void)
{
    const bms_protect_status_t *protect;
    bool usb;
    uint32_t previousFaults;

    if (!s_initialized)
    {
        return;
    }

    /* -------------------------------------------------------------------------------------------------------------- */
    /* USB 插入检测去抖                                                                                                */
    /* -------------------------------------------------------------------------------------------------------------- */

    usb = BMS_IO_USB_IS_PRESENT();

    /*
     * 检测到 USB 状态发生变化时，
     * 重新开始稳定时间计数。
     */
    if (usb != s_usbCandidate)
    {
        s_usbCandidate = usb;
        s_usbStableMs = 1U;
    }
    else if (s_usbStableMs < BMS_CTRL_USB_DEBOUNCE_MS)
    {
        s_usbStableMs++;
    }

    /*
     * USB 状态持续稳定达到去抖时间后，
     * 才更新对外状态。
     */
    if (s_usbStableMs >= BMS_CTRL_USB_DEBOUNCE_MS)
    {
        s_status.usbPresent = s_usbCandidate;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* IP5326 LED 状态采集                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 当前硬件只将 IP5326 LED1 / LED3 信号接入 MCU，
     * 因此这里只记录实际 GPIO 电平，
     * 不直接按照 IP5326 四灯显示逻辑推算电量。
     */
    s_status.led1Level = BMS_IO_LED1_IS_HIGH();
    s_status.led3Level = BMS_IO_LED3_IS_HIGH();


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 电池保护状态更新                                                                                                */
    /* -------------------------------------------------------------------------------------------------------------- */

    bms_protect_1ms_task();

    protect = bms_protect_GetStatus();

    previousFaults = s_status.faults;
    s_status.faults = protect->faults;


    /* -------------------------------------------------------------------------------------------------------------- */
    /* KEY 操作保护                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 放电被禁止时，如果当前正在执行非 Standby KEY 操作，
     * 立即释放 KEY，并进入保护等待阶段。
     *
     * Standby 命令允许继续执行。
     */
    if (!protect->dischargeAllowed &&
        (s_keyCommand != BMS_KEY_STANDBY) &&
        ((s_keyPhase == BMS_KEY_FIRST_PRESS) ||
         (s_keyPhase == BMS_KEY_DOUBLE_GAP)  ||
         (s_keyPhase == BMS_KEY_SECOND_PRESS)))
    {
        bms_ctrl_SetKey(false);

        s_keyPhase = BMS_KEY_GUARD;
        s_keyRemainingMs = BMS_CTRL_KEY_GUARD_MS;
        s_status.keyBusy = true;
    }


    /*
     * 推进 KEY 状态机。
     */
    bms_ctrl_KeyTask();


    /*
     * 根据最新保护状态重新计算 VIN/VBAT 实际输出。
     */
    bms_ctrl_ApplyOutputs();


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 故障变化通知                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    if (previousFaults != s_status.faults)
    {
        bms_ctrl_FaultChangedCallback(s_status.faults);
    }
}


/*******************************************************************************
//  Function   : bms_ctrl_SetVinEnabled
//  Description: 请求开启或关闭 VIN 输入通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：模块未初始化或请求被保护逻辑限制
//  Others     : VIN 是否真正开启还受到 chargeAllowed 状态限制
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVinEnabled(bool enabled)
{
    if (!s_initialized)
    {
        return false;
    }

    s_vinRequested = enabled;

    bms_ctrl_ApplyOutputs();

    return (s_status.vinEnabled == enabled);
}


/*******************************************************************************
//  Function   : bms_ctrl_SetVbatEnabled
//  Description: 请求开启或关闭 VBAT 电池通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：模块未初始化或请求被保护/待机逻辑限制
//  Others     : VBAT 是否真正开启还受到 standbyRequested 和 dischargeAllowed 状态限制
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVbatEnabled(bool enabled)
{
    if (!s_initialized)
    {
        return false;
    }

    s_vbatRequested = enabled;

    bms_ctrl_ApplyOutputs();

    return (s_status.vbatEnabled == enabled);
}


/*******************************************************************************
//  Function   : bms_ctrl_EnterStandby
//  Description: 请求 BMS 控制模块进入待机状态
//  Param      : None
//  Return     : None
//  Others     : 根据待机配置保持或关闭 VIN 输入通路，同时关闭 VBAT 通路并释放 KEY
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_EnterStandby(void)
{
    if (!s_initialized)
    {
        return;
    }

    /*
     * 标记系统进入待机状态。
     */
    s_status.standbyRequested = true;

    /*
     * VIN 是否保持开启由待机配置宏决定。
     *
     * 通常可用于保留 USB 输入及充电能力。
     */
    s_vinRequested = BMS_CTRL_STANDBY_VIN_ENABLED;

    /*
     * 待机期间关闭 VBAT 输出通路。
     */
    s_vbatRequested = false;

    /*
     * 强制释放 KEY。
     */
    bms_ctrl_SetKey(false);

    /*
     * 进入 KEY 保护阶段，避免待机切换期间立即触发新的 KEY 操作。
     */
    s_keyPhase = BMS_KEY_GUARD;
    s_keyRemainingMs = BMS_CTRL_KEY_GUARD_MS;
    s_status.keyBusy = true;

    /*
     * 更新实际 VIN/VBAT 输出。
     */
    bms_ctrl_ApplyOutputs();
}


/*******************************************************************************
//  Function   : bms_ctrl_LeaveStandby
//  Description: 清除 BMS 待机请求状态
//  Param      : None
//  Return     : None
//  Others     : 仅清除 standbyRequested 标志，不主动恢复 VBAT 请求状态
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_LeaveStandby(void)
{
    s_status.standbyRequested = false;
}


/*******************************************************************************
//  Function   : bms_ctrl_RequestKey
//  Description: 请求执行一次 IP5326 KEY 控制命令
//  Param      : command - KEY 控制命令
//  Return     : true：命令已成功启动；false：当前状态不允许执行
//  Others     : 支持短按、长按及双击操作，实际时序由 1ms KEY 状态机完成
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_RequestKey(bms_key_command_t command)
{
    /*
     * 以下情况拒绝新的 KEY 命令：
     *
     * 1. 模块尚未初始化
     * 2. 已有 KEY 命令正在执行
     * 3. command 超出有效范围
     */
    if (!s_initialized ||
        (s_keyPhase != BMS_KEY_IDLE) ||
        ((unsigned int)command > (unsigned int)BMS_KEY_LIGHT_TOGGLE))
    {
        return false;
    }

    /*
     * 非 Standby 命令执行条件：
     *
     * 1. 当前不能处于待机请求状态
     * 2. 电池保护模块必须允许放电
     *
     * Standby 命令不受该条件限制。
     */
    if ((command != BMS_KEY_STANDBY) &&
        (s_status.standbyRequested ||
         !bms_protect_GetStatus()->dischargeAllowed))
    {
        return false;
    }

    /*
     * 记录本次 KEY 命令。
     */
    s_keyCommand = command;

    /*
     * 所有 KEY 命令均从第一次按键阶段开始。
     */
    s_keyPhase = BMS_KEY_FIRST_PRESS;

    /*
     * Standby 使用长按时间，
     * 其他命令第一次按键使用短按时间。
     */
    s_keyRemainingMs = (command == BMS_KEY_STANDBY) ?
                       BMS_CTRL_KEY_LONG_MS :
                       BMS_CTRL_KEY_SHORT_MS;

    s_status.keyBusy = true;

    /*
     * 开始模拟按键按下。
     */
    bms_ctrl_SetKey(true);

    return true;
}


/*******************************************************************************
//  Function   : bms_ctrl_GetStatus
//  Description: 获取 BMS 控制模块当前状态
//  Param      : None
//  Return     : BMS 控制状态结构体只读指针
//  Others     : 返回内部静态状态对象，调用方不应修改其内容
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_ctrl_status_t *bms_ctrl_GetStatus(void)
{
    return &s_status;
}
