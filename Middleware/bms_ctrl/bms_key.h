/**
 ***********************************************************************************************************************
 * @file    bms_key.h
 * @brief   IP5326 KEY 非阻塞状态机接口定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-14
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_KEY_H_
#define _BMS_KEY_H_

#include "bms_config.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** IP5326 KEY 小于该时间通常不响应，单位 ms */
#define BMS_KEY_IGNORE_MAX_MS           70U

/** IP5326 KEY 短按最小有效时间，单位 ms */
#define BMS_KEY_SHORT_MIN_MS           100U

/** IP5326 KEY 长按判定阈值，单位 ms */
#define BMS_KEY_LONG_THRESHOLD_MS     2000U

/** IP5326 KEY 双击允许的最大时间窗口，单位 ms */
#define BMS_KEY_DOUBLE_WINDOW_MS      1000U


#if BMS_MODULE_KEY_ENABLE

/**
 * @brief IP5326 KEY GPIO 控制
 *
 * MCU 输出高电平时 Q4 导通，将 IP5326 KEY 拉低，模拟按键按下；
 * MCU 输出低电平时 Q4 截止，释放 IP5326 KEY。
 */
#define BMS_IO_KEY_PRESS()          LL_GPIO_SetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)
#define BMS_IO_KEY_RELEASE()        LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN)


/* GPIO configuration check ------------------------------------------------------------------------------------------*/

/** KEY 模块启用时必须配置 KEY GPIO。 */
#if !defined(BMS_KEY_PORT) || !defined(BMS_KEY_PIN)
#error "BMS KEY module requires BMS_KEY_PORT and BMS_KEY_PIN mappings in bsp_gpio.h"
#endif


/* KEY timing check --------------------------------------------------------------------------------------------------*/

/** 短按必须大于 100ms。 */
#if (BMS_CTRL_KEY_SHORT_MS <= BMS_KEY_SHORT_MIN_MS)
#error "BMS_CTRL_KEY_SHORT_MS must be greater than 100 ms"
#endif

/** 短按必须小于 2000ms。 */
#if (BMS_CTRL_KEY_SHORT_MS >= BMS_KEY_LONG_THRESHOLD_MS)
#error "BMS_CTRL_KEY_SHORT_MS must be less than 2000 ms"
#endif

/** 长按必须大于 2000ms。 */
#if (BMS_CTRL_KEY_LONG_MS <= BMS_KEY_LONG_THRESHOLD_MS)
#error "BMS_CTRL_KEY_LONG_MS must be greater than 2000 ms"
#endif

/** 双击动作必须在 1s 内完成。 */
#if ((BMS_CTRL_KEY_SHORT_MS * 2U + BMS_CTRL_KEY_DOUBLE_GAP_MS) >= BMS_KEY_DOUBLE_WINDOW_MS)
#error "IP5326 double click sequence must finish within 1000 ms"
#endif

#endif /* BMS_MODULE_KEY_ENABLE */


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief KEY 状态机阶段
 */
typedef enum
{
    BMS_KEY_IDLE = 0,          /**< 空闲状态 */
    BMS_KEY_FIRST_PRESS,       /**< 第一次短按或长按阶段 */
    BMS_KEY_DOUBLE_GAP,        /**< 双击两次按键之间的释放间隔 */
    BMS_KEY_SECOND_PRESS,      /**< 双击第二次短按阶段 */
    BMS_KEY_GUARD              /**< KEY 动作结束后的释放保护阶段 */
} bms_key_phase_t;


/**
 * @brief KEY 控制命令
 *
 * 类型始终可见，KEY 模块关闭后其它模块仍可保持接口兼容。
 */
typedef enum
{
    BMS_KEY_WAKEUP = 0,        /**< 短按，唤醒并请求开启升压 */
    BMS_KEY_STANDBY,           /**< 长按，请求进入待机 */
    BMS_KEY_LIGHT_TOGGLE       /**< 双击，切换照明状态 */
} bms_key_command_t;


/**
 * @brief KEY 状态机运行状态
 */
typedef struct
{
    bool busy;                 /**< KEY 状态机忙标志，包括按键、双击间隔及 Guard 阶段 */
    bool pressed;              /**< 当前软件模拟 KEY 按下状态 */
    bool allowed;              /**< 非待机命令执行权限 */
    bms_key_command_t command; /**< 最近一次接受的命令，不代表芯片实际执行结果 */
} bms_key_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

#if BMS_MODULE_KEY_ENABLE

/*******************************************************************************
//  Function   : bms_key_Init
//  Description: 初始化 KEY 状态机
//  Param      : None
//  Return     : true：初始化成功；false：KEY GPIO 配置错误
//  Others     : 初始化后释放 KEY，仅检查 KEY 引脚，不依赖其他 BMS 模块
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_key_Init(void);


/*******************************************************************************
//  Function   : bms_key_1ms_task
//  Description: KEY 状态机 1ms 周期任务
//  Param      : None
//  Return     : None
//  Others     : 完成短按、长按、双击及 Guard 阶段时序推进，不可重复调度
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_1ms_task(void);


/*******************************************************************************
//  Function   : bms_key_Request
//  Description: 请求执行一次 KEY 操作
//  Param      : command - KEY 控制命令
//  Return     : true：命令已接受；false：未初始化、忙、命令非法或权限不足
//  Others     : Standby 命令不受非待机命令权限限制
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_key_Request(bms_key_command_t command);


/*******************************************************************************
//  Function   : bms_key_SetAllowed
//  Description: 设置非待机 KEY 命令执行权限
//  Param      : allowed - true：允许；false：禁止
//  Return     : None
//  Others     : 禁止时取消正在执行的非待机命令并进入 Guard，Standby 命令可继续执行
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_SetAllowed(bool allowed);


/*******************************************************************************
//  Function   : bms_key_Cancel
//  Description: 取消当前 KEY 操作
//  Param      : None
//  Return     : None
//  Others     : 立即释放 KEY 并进入 Guard，不改变芯片电源通路状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_Cancel(void);


/*******************************************************************************
//  Function   : bms_key_GetStatus
//  Description: 获取当前 KEY 状态机状态
//  Param      : None
//  Return     : 指向内部 bms_key_status_t 状态结构体的只读指针
//  Others     : 返回状态仅表示软件 KEY 命令，不代表芯片实际功能状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_key_status_t *bms_key_GetStatus(void);

#endif /* BMS_MODULE_KEY_ENABLE */


#endif /* _BMS_KEY_H_ */
