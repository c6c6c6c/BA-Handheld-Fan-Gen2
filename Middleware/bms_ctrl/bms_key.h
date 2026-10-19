/**
 ***********************************************************************************************************************
 * @file    bms_key.h
 * @brief   IP5326 KEY 非阻塞控制接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_KEY_H_
#define _BMS_KEY_H_

#include "bms_config.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief IP5326 KEY GPIO 控制
 *
 * MCU 输出高电平时 Q4 导通，将 IP5326 KEY 拉低，模拟按键按下；
 * MCU 输出低电平时 Q4 截止，释放 IP5326 KEY。
 */
#define BMS_IO_KEY_PRESS()          LL_GPIO_SetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)
#define BMS_IO_KEY_RELEASE()        LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)

/** KEY 命令执行完成后的保护间隔，单位 ms */
#define BMS_KEY_GUARD_TIME_MS       2000U

/** KEY 空闲状态对应的软件按下状态 */
#define BMS_KEY_IDLE_IO             true


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief KEY 状态机命令及内部状态
 */
typedef enum
{
    BMS_KEY_WAKEUP = 0,            /**< 短按，唤醒电量显示并请求开启升压输出 */
    BMS_KEY_STANDBY,               /**< 长按，请求进入待机状态 */
    BMS_KEY_LIGHT_TOGGLE,          /**< 双击，切换照明灯状态 */

    BMS_KEY_NUM,                   /**< KEY 有效控制命令数量 */
    BMS_KEY_GUARD,                 /**< Guard 保护阶段 */
    BMS_KEY_LIGHT_IDEL             /**< KEY 状态机空闲状态 */
} bms_key_command_t;


/**
 * @brief 单个 KEY 命令的时序配置
 */
typedef struct
{
    uint16_t count;                /**< times 数组有效元素数量 */
    const uint16_t *times;         /**< 各阶段持续时间数组，单位 ms */
} bms_key_process_t;


/**
 * @brief KEY 状态机运行状态
 */
typedef struct
{
    bool pressed;                       /**< 当前 MCU 模拟 KEY 按下状态 */
    uint32_t step_times;                /**< 当前阶段已经运行时间，单位 ms */
    uint16_t step;                      /**< 当前执行阶段索引 */
    bms_key_command_t target;           /**< 当前待执行目标命令 */
    bms_key_command_t execute;          /**< 当前正在执行的命令或状态 */
    bms_key_command_t lastComplete;     /**< 当前正在执行的命令或状态 */
} bms_key_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_key_Init
//  Description: 初始化 IP5326 KEY 非阻塞状态机
//  Param      : None
//  Return     : None
//  Others     : 将状态机恢复为空闲状态，并清零阶段索引和阶段计时
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_key_Init(void);


/*******************************************************************************
//  Function   : bms_key_1ms_task
//  Description: 周期推进 IP5326 KEY 非阻塞状态机
//  Param      : None
//  Return     : None
//  Others     : 建议每 1 ms 调用一次，根据命令对应的 times[] 完成 KEY 电平翻转及 Guard 时序
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_key_1ms_task(void);


/*******************************************************************************
//  Function   : bms_key_Request
//  Description: 请求执行一次 IP5326 KEY 控制命令
//  Param      : command - KEY 控制命令
//  Return     : None
//  Others     : 仅更新目标命令，状态机进入空闲状态后开始执行
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_key_Request(bms_key_command_t command);


/*******************************************************************************
//  Function   : bms_key_RequestForce
//  Description: 强制请求执行一次 IP5326 KEY 控制命令
//  Param      : command - KEY 控制命令
//  Return     : None
//  Others     : 当前动作先进入 Guard，Guard 结束后再执行新的目标命令
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_key_RequestForce(bms_key_command_t command);


/*******************************************************************************
//  Function   : bms_key_GetStatus
//  Description: 获取 KEY 状态机当前运行状态
//  Param      : None
//  Return     : 指向内部 bms_key_status_t 状态结构体的只读指针
//  Others     : 仅表示 MCU 模拟 KEY 的执行状态，不代表 IP5326 实际内部工作状态
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
const bms_key_status_t *bms_key_GetStatus(void);


#endif /* _BMS_KEY_H_ */
