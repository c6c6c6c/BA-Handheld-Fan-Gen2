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
 */
typedef enum
{
    BMS_KEY_WAKEUP = 0,      /**< 短按 KEY，唤醒并请求开启升压输出 */
    BMS_KEY_STANDBY,         /**< 长按 KEY，请求进入待机状态 */
    BMS_KEY_LIGHT_TOGGLE     /**< 双击 KEY，切换照明灯状态 */
} bms_key_command_t;


/**
 * @brief BMS 控制模块运行状态
 */
typedef struct
{
    bool usbPresent;         /**< USB 去抖后的稳定插入状态 */
    bool led1Level;          /**< IP5326 LED1 原始输入电平 */
    bool led3Level;          /**< IP5326 LED3 原始输入电平 */

    bool vinEnabled;         /**< VIN_EN 当前软件输出命令状态，不代表硬件通路反馈 */
    bool vbatEnabled;        /**< VBAT_EN 当前软件输出命令状态，不代表硬件通路反馈 */

    bool standbyRequested;   /**< 上层待机请求状态，待机时限制 VBAT 通路 */
    bool keyBusy;            /**< KEY 状态机忙标志，包括按键、双击间隔及 Guard 阶段 */

    uint32_t faults;         /**< 当前有效故障和锁存故障的并集 */
} bms_ctrl_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_ctrl_Init
//  Description: 初始化 BMS 控制模块
//  Param      : protectConfig - 软件保护配置；传入 NULL 时使用 bms_config.h 默认配置
//  Return     : true：初始化成功；false：GPIO 状态或保护配置无效
//  Others     : 应在 bsp_gpio_Init() 后调用；VIN/VBAT 初始请求由 BMS_CTRL_INIT_xxx 宏决定
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_Init(const bms_protect_config_t *protectConfig);


/*******************************************************************************
//  Function   : bms_ctrl_1ms_task
//  Description: BMS 控制模块 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成 USB 去抖、LED 状态采集、保护处理、KEY 状态机及 VIN/VBAT 输出更新
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_1ms_task(void);


/*******************************************************************************
//  Function   : bms_ctrl_SetVinEnabled
//  Description: 请求开启或关闭 VIN 输入通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：请求未满足或模块未初始化
//  Others     : 开启请求受 chargeAllowed 联锁限制，关闭请求可直接执行
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVinEnabled(bool enabled);


/*******************************************************************************
//  Function   : bms_ctrl_SetVbatEnabled
//  Description: 请求开启或关闭 VBAT 电池通路
//  Param      : enabled - true：请求开启；false：请求关闭
//  Return     : true：最终输出状态与请求一致；false：请求未满足或模块未初始化
//  Others     : 开启请求受 standbyRequested 和 dischargeAllowed 联锁限制
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_SetVbatEnabled(bool enabled);


/*******************************************************************************
//  Function   : bms_ctrl_EnterStandby
//  Description: 请求 BMS 控制模块进入待机状态
//  Param      : None
//  Return     : None
//  Others     : 关闭 VBAT 请求，VIN 是否保留由 BMS_CTRL_STANDBY_VIN_ENABLED 决定
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_EnterStandby(void);


/*******************************************************************************
//  Function   : bms_ctrl_LeaveStandby
//  Description: 退出 BMS 控制模块待机请求状态
//  Param      : None
//  Return     : None
//  Others     : 仅清除 standbyRequested，不自动重新开启 VIN/VBAT 或触发 KEY
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_LeaveStandby(void);


/*******************************************************************************
//  Function   : bms_ctrl_RequestKey
//  Description: 请求执行一次 IP5326 KEY 操作
//  Param      : command - KEY 控制命令
//  Return     : true：命令已启动；false：当前状态不允许执行
//  Others     : KEY 操作采用非阻塞状态机完成，忙状态下拒绝新的 KEY 命令
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
bool bms_ctrl_RequestKey(bms_key_command_t command);


/*******************************************************************************
//  Function   : bms_ctrl_GetStatus
//  Description: 获取 BMS 控制模块当前状态
//  Param      : None
//  Return     : 指向内部 bms_ctrl_status_t 的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
const bms_ctrl_status_t *bms_ctrl_GetStatus(void);


/*******************************************************************************
//  Function   : bms_ctrl_FaultChangedCallback
//  Description: BMS 故障状态变化回调函数
//  Param      : faults - 当前有效及锁存故障集合
//  Return     : None
//  Others     : Weak 回调，可由上层重新实现，用于停止风机、雾化等高功率负载
//  Author     : Chen Liang 2026-10-13
*******************************************************************************/
void bms_ctrl_FaultChangedCallback(uint32_t faults);


#endif /* _BMS_CTRL_H_ */