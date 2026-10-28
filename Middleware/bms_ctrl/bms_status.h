
/**
 ***********************************************************************************************************************
 * @file    bms_status.h
 * @brief   BMS 状态识别接口，支持 IP5326 单灯、双灯及四灯模式的 LED 状态检测
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-14
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BMS_STATUS_H_
#define _BMS_STATUS_H_

#include "bms_config.h"
#include "timer_base.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** IP5326 单灯状态识别模式 */
#define BMS_STATUS_MODE_1_LED          1U

/** IP5326 双灯状态识别模式 */
#define BMS_STATUS_MODE_2_LED          2U

/** IP5326 四灯状态识别模式 */
#define BMS_STATUS_MODE_4_LED          4U


/** 默认使用双灯状态识别模式 */
#ifndef BMS_STATUS_LED_MODE
#define BMS_STATUS_LED_MODE            BMS_STATUS_MODE_2_LED
#endif


/** LED 状态统计窗口，单位 ms */
#define BMS_STATUS_DETECT_WINDOW_MS    2000U


/* GPIO Control ------------------------------------------------------------------------------------------------------*/

/** 读取 LED1 当前 GPIO 电平，true=高电平，false=低电平 */
#define BMS_IO_LED1_IS_HIGH()          (LL_GPIO_IsInputPinSet(BMS_LED1_PORT, BMS_LED1_PIN) != 0U)


/* 双灯或四灯模式下启用 LED2 电平检测 */
#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

/** 读取 LED2 当前 GPIO 电平 */
#define BMS_IO_LED2_IS_HIGH()          (LL_GPIO_IsInputPinSet(BMS_LED2_PORT, BMS_LED2_PIN) != 0U)

#endif


/* 四灯模式下额外启用 LED3、LED4 电平检测 */
#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

/** 读取 LED3 当前 GPIO 电平 */
#define BMS_IO_LED3_IS_HIGH()          (LL_GPIO_IsInputPinSet(BMS_LED3_PORT, BMS_LED3_PIN) != 0U)

/** 读取 LED4 当前 GPIO 电平 */
#define BMS_IO_LED4_IS_HIGH()          (LL_GPIO_IsInputPinSet(BMS_LED4_PORT, BMS_LED4_PIN) != 0U)

#endif


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 单路 LED 工作状态
 * @note  根据 LED 电平变化及统计窗口内的边沿信息，
 *        区分常亮、常灭、0.5Hz 闪烁和 1Hz 闪烁。
 */
typedef enum
{
    LED_STATE_UNKNOWN = 0,         /**< 状态未知或尚未完成检测 */
    LED_STATE_OFF,                 /**< LED 持续熄灭 */
    LED_STATE_ON,                  /**< LED 持续点亮 */
    LED_STATE_BLINK_0_5HZ,         /**< LED 以 0.5Hz 闪烁 */
    LED_STATE_BLINK_1HZ            /**< LED 以 1.0Hz 闪烁 */
} led_state_t;


/**
 * @brief IP5326 工作状态枚举
 * @note  根据配置的 LED 模式及检测结果判断充电、放电、
 *        低电量、待机等状态；具体组合由 bms_status.c 实现。
 */
typedef enum
{
    IP5326_STATUS_UNKNOWN = 0,     /**< 状态未知或检测尚未完成 */
    IP5326_STATUS_CHARGING,        /**< 充电状态 */
    IP5326_STATUS_FULL,            /**< 充电完成状态 */
    IP5326_STATUS_DISCHARGING,     /**< 正常放电状态 */
    IP5326_STATUS_LOW_BATTERY,     /**< 低电量放电状态 */
    IP5326_STATUS_IDLE,            /**< 空闲状态，LED 全灭 */
    IP5326_STATUS_LOCKED,          /**< 低电量锁定状态 */
    IP5326_STATUS_INVALID          /**< LED 状态组合异常或无法识别 */
} ip5326_status_t;


/**
 * @brief 单路 LED 状态检测数据
 * @note  保存 GPIO 采样电平、边沿统计及状态检测计时信息，
 *        用于识别 LED 常亮、常灭与不同频率的闪烁状态。
 */
typedef struct
{
    uint8_t      level;            /**< 当前采样得到的 LED 电平 */
    uint8_t      last_level;       /**< 上一次采样得到的 LED 电平 */

    uint8_t      count;            /**< LED 状态检测辅助计数值 */
    uint8_t      edge_count;       /**< 检测窗口内累计的有效电平边沿数 */

    led_state_t  state;            /**< 当前识别得到的 LED 状态 */
    timer_base_t _timer;           /**< LED 状态检测计时器 */
} led_detect_t;


/**
 * @brief IP5326 LED 状态识别结果
 * @note  根据 BMS_STATUS_LED_MODE 配置检测通道数量，
 *        并将各路 LED 识别结果组合为 IP5326 工作状态。
 */
typedef struct
{
    led_detect_t led1;             /**< LED1 状态检测数据 */

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    led_detect_t led2;             /**< LED2 状态检测数据 */

#endif

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    led_detect_t led3;             /**< LED3 状态检测数据 */
    led_detect_t led4;             /**< LED4 状态检测数据 */

#endif

    ip5326_status_t state;         /**< 根据各路 LED 状态识别出的 IP5326 工作状态 */
} bms_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_status_check
//  Description: 更新 LED 检测数据并识别 IP5326 工作状态
//  Param      : state - 当前电池运行状态
//  Return     : None
//  Others     : 根据 BMS_STATUS_LED_MODE 选择单灯、双灯或四灯识别逻辑；
//               结合 LED 电平、闪烁状态及电池运行状态生成识别结果
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_status_check(battery_state_t state);


/*******************************************************************************
//  Function   : bms_status_GetStatus
//  Description: 获取当前 IP5326 状态识别结果
//  Param      : None
//  Return     : 指向内部 bms_status_t 状态数据的只读指针
//  Others     : 返回对象由状态识别模块维护，调用方不得直接修改；
//               可通过 state 及各 LED 成员查询当前识别结果
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_status_t *bms_status_GetStatus(void);


#endif /* _BMS_STATUS_H_ */
