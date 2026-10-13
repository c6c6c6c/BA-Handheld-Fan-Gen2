/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.h
 * @brief   BMS 控制模块接口定义，实现 IP5326 GPIO 控制、非阻塞 KEY 操作及软件保护输出联锁
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-13
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_CTRL_H_
#define _BMS_CTRL_H_

#include "bms_protect.h"


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief IP5326 KEY 控制命令
 *
 * KEY 操作通过非阻塞状态机完成，
 * 实际按键时序由 bms_config.h 中的 BMS_CTRL_KEY_xxx_MS 参数决定。
 *
 * @note
 * KEY 命令只表示向 IP5326 发出对应按键动作，
 * 不代表相关硬件功能一定已经成功执行。
 */
typedef enum
{
    /**
     * @brief 短按 KEY
     *
     * 用于唤醒 IP5326、显示电量并请求开启升压输出。
     *
     * @note
     * 当前模块不会检测升压输出是否真正建立。
     */
    BMS_KEY_WAKEUP = 0,

    /**
     * @brief 长按 KEY
     *
     * 用于请求 IP5326 进入待机状态。
     *
     * @note
     * 长按进入待机时，不保证照明灯状态同步关闭。
     */
    BMS_KEY_STANDBY,

    /**
     * @brief 双击 KEY
     *
     * 在规定时间内执行两次短按，
     * 用于切换 IP5326 照明灯状态。
     *
     * @note
     * 当前硬件没有独立照明灯状态反馈，
     * 因此模块无法确认最终灯状态。
     */
    BMS_KEY_LIGHT_TOGGLE

} bms_key_command_t;


/**
 * @brief BMS 控制模块运行状态
 *
 * 保存 USB 输入状态、IP5326 LED 原始电平、
 * VIN/VBAT 命令状态、待机状态、KEY 状态及当前故障信息。
 *
 * @note
 * vinEnabled / vbatEnabled 表示软件最终输出命令状态，
 * 不代表实际功率通路已经成功导通。
 */
typedef struct
{
    /**
     * @brief USB 输入稳定状态
     *
     * USB_IN 连续保持相同电平达到
     * BMS_CTRL_USB_DEBOUNCE_MS 后才更新。
     */
    bool usbPresent;

    /**
     * @brief IP5326 LED1 原始输入电平
     *
     * 仅表示 MCU 当前读取到的 GPIO 电平，
     * 不直接等价于某一颗 LED 的亮灭状态或精确电量百分比。
     */
    bool led1Level;

    /**
     * @brief IP5326 LED3 原始输入电平
     *
     * 仅表示 MCU 当前读取到的 GPIO 电平，
     * 不直接等价于精确电量或充电状态。
     */
    bool led3Level;

    /**
     * @brief VIN 输入通路当前命令状态
     *
     * true：
     * 当前软件最终允许并输出 VIN_EN 有效电平。
     *
     * false：
     * 当前 VIN_EN 被关闭。
     *
     * @note
     * 该状态不是硬件反馈。
     */
    bool vinEnabled;

    /**
     * @brief VBAT 电池通路当前命令状态
     *
     * true：
     * 当前软件最终允许并输出 VBAT_EN 有效电平。
     *
     * false：
     * 当前 VBAT_EN 被关闭。
     *
     * @note
     * 该状态不是硬件反馈。
     */
    bool vbatEnabled;

    /**
     * @brief 上层待机请求状态
     *
     * true：
     * 当前系统处于 BMS 待机请求状态，
     * VBAT 通路将受到限制。
     *
     * false：
     * 当前没有待机请求。
     */
    bool standbyRequested;

    /**
     * @brief KEY 状态机忙标志
     *
     * 包括：
     *
     * - 短按保持阶段
     * - 长按保持阶段
     * - 双击间隔
     * - 第二次短按
     * - 动作结束后的 Guard 保护阶段
     */
    bool keyBusy;

    /**
     * @brief 当前 BMS 故障集合
     *
     * 包含当前有效故障以及锁存故障。
     *
     * 对应 BMS_PROTECT_xxx 故障位定义。
     */
    uint32_t faults;

} bms_ctrl_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_ctrl_Init
//  Description: 初始化 BMS 控制模块
//  Param      : protectConfig - 软件保护配置；传入 NULL 时使用 bms_config.h 默认配置
//  Return     : true：初始化成功；false：GPIO状态或保护配置无效
//  Others     : 应在 bsp_gpio_Init() 完成后调用；初始化过程中释放 KEY，
//               VIN/VBAT 初始请求状态由 BMS_CTRL_INIT_xxx 宏决定，
//               CTL 保持 bsp_gpio_Init() 设置的默认状态
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_Init(const bms_protect_config_t *protectConfig);


/*******************************************************************************
//  Function   : bms_ctrl_1ms_task
//  Description: BMS 控制模块 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成 USB 去抖、LED 状态采集、软件保护、KEY 状态机、
//               VIN/VBAT 输出联锁及故障变化检测；
//               内部已调用 bms_protect_1ms_task()，外部不要重复调度
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_1ms_task(void);


/*******************************************************************************
//  Function   : bms_ctrl_SetVinEnabled
//  Description: 请求开启或关闭 VIN 输入通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：请求未能满足或模块未初始化
//  Others     : 开启请求仍受软件保护 chargeAllowed 联锁限制；
//               关闭请求不依赖 chargeAllowed，可直接关闭 VIN 通路
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVinEnabled(bool enabled);


/*******************************************************************************
//  Function   : bms_ctrl_SetVbatEnabled
//  Description: 请求开启或关闭 VBAT 电池通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：请求未能满足或模块未初始化
//  Others     : 开启请求受 standbyRequested 和 dischargeAllowed 联锁限制；
//               关闭请求可直接关闭 VBAT 通路
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVbatEnabled(bool enabled);


/*******************************************************************************
//  Function   : bms_ctrl_EnterStandby
//  Description: 请求 BMS 控制模块进入待机状态
//  Param      : None
//  Return     : None
//  Others     : 待机时关闭 VBAT 请求，VIN 是否保留由 BMS_CTRL_STANDBY_VIN_ENABLED 决定；
//               同时释放 KEY 并进入短暂 Guard 保护阶段
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_EnterStandby(void);


/*******************************************************************************
//  Function   : bms_ctrl_LeaveStandby
//  Description: 退出 BMS 控制模块待机请求状态
//  Param      : None
//  Return     : None
//  Others     : 仅清除 standbyRequested，
//               不自动重新开启 VIN/VBAT，也不会主动触发 KEY 操作
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_LeaveStandby(void);


/*******************************************************************************
//  Function   : bms_ctrl_RequestKey
//  Description: 请求执行一次 IP5326 KEY 操作
//  Param      : command - KEY 控制命令
//  Return     : true：命令已启动；false：当前状态不允许执行
//  Others     : KEY 操作采用非阻塞状态机完成；忙状态下拒绝新的 KEY 命令；
//               USB 板 Q4 为反相驱动，MCU KEY 输出高电平时模拟 IP5326 KEY 按下
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_RequestKey(bms_key_command_t command);


/*******************************************************************************
//  Function   : bms_ctrl_GetStatus
//  Description: 获取 BMS 控制模块当前状态
//  Param      : None
//  Return     : 指向内部 bms_ctrl_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改其内容
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_ctrl_status_t *bms_ctrl_GetStatus(void);


/*******************************************************************************
//  Function   : bms_ctrl_FaultChangedCallback
//  Description: BMS 故障状态变化回调函数
//  Param      : faults - 当前有效及锁存故障集合
//  Return     : None
//  Others     : Weak 回调，可由上层重新实现；建议在此停止风机、雾化等高功率负载，
//               并由上层决定故障恢复后何时允许重新启动；
//               关闭 VBAT_EN 不代表 USB 输入供电的负载一定断电；
//               当前 IP3108A CTL 有效极性未确认，因此模块不自动控制 BMS_CTL
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_FaultChangedCallback(uint32_t faults);


#endif /* _BMS_CTRL_H_ */
