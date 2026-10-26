/**
 ***********************************************************************************************************************
 * @file    bms_status.h
 * @brief   BMS 状态识别接口，实现 IP5326 1/2/4 灯模式下的 LED 状态及充放电状态识别
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

/* IP5326 单灯模式 */
#define BMS_STATUS_MODE_1_LED       1U

/* IP5326 双灯模式 */
#define BMS_STATUS_MODE_2_LED       2U

/* IP5326 四灯模式 */
#define BMS_STATUS_MODE_4_LED       4U


/* 默认使用双灯状态识别模式 */
#ifndef BMS_STATUS_LED_MODE
#define BMS_STATUS_LED_MODE         BMS_STATUS_MODE_2_LED
#endif


/** LED 状态统计窗口，单位 ms。 */
#define BMS_STATUS_DETECT_WINDOW_MS    2000U

/* LED GPIO 状态读取 -------------------------------------------------------------------------------------------------*/

#define BMS_IO_LED1_IS_HIGH()       (LL_GPIO_IsInputPinSet(BMS_LED1_PORT, BMS_LED1_PIN) != 0U)

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    #define BMS_IO_LED2_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED2_PORT, BMS_LED2_PIN) != 0U)

#elif (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    #define BMS_IO_LED3_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED3_PORT, BMS_LED3_PIN) != 0U)
    #define BMS_IO_LED4_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED4_PORT, BMS_LED4_PIN) != 0U)

#endif



/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 单路 LED 识别状态
 */
typedef enum
{
    LED_STATE_UNKNOWN = 0,      /**< 状态未知或仍处于观察阶段 */
    LED_STATE_OFF,              /**< LED 常灭 */
    LED_STATE_ON,               /**< LED 常亮 */
    LED_STATE_BLINK_0_5HZ,      /**< LED 0.5Hz 闪烁 */
    LED_STATE_BLINK_1HZ         /**< LED 1.0Hz 闪烁 */
} led_state_t;


/**
 * @brief BMS 充放电状态
 */
typedef enum
{
    IP5326_STATUS_UNKNOWN = 0,     /**< 状态未知或检测尚未完成 */
    IP5326_STATUS_CHARGING,        /**< 充电中，LED1 0.5Hz 闪烁 */
    IP5326_STATUS_FULL,            /**< 充电完成，LED1 常亮 */
    IP5326_STATUS_DISCHARGING,     /**< 正常放电，LED3 常亮 */
    IP5326_STATUS_LOW_BATTERY,     /**< 低电量放电，LED3 1.0Hz 闪烁 */
    IP5326_STATUS_IDLE,            /**< LED 全灭，可能为待机或关机 */
    IP5326_STATUS_LOCKED,          /**< 低电锁定，需结合其他信号确认 */
    IP5326_STATUS_INVALID          /**< LED 组合异常或不符合当前模式 */
} ip5326_status_t;


/**
 * @brief LED 闪烁检测数据
 */
typedef struct
{
	uint8_t         level;             /**< 上一次采样电平 */
    uint8_t         last_level;        /**< 上一次采样电平 */
	uint8_t         count;             /**< 已检测到的有效边沿数量 */
    uint8_t         edge_count;        /**< 已检测到的有效边沿数量 */
    led_state_t     state;
    timer_base_t   _timer;
} led_detect_t;


/**
 * @brief BMS LED 状态识别结果
 */
typedef struct
{
    led_detect_t led1;               /**< LED1 状态 */

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)
    led_detect_t led2;               /**< LED2 状态 */
#endif

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)
    led_detect_t led3;               /**< LED3 状态 */
    led_detect_t led4;               /**< LED4 状态 */
#endif

    ip5326_status_t state;           /**< 根据 LED 组合识别出的当前 BMS 状态 */
} bms_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_status_check
//  Description: 更新 LED 状态并识别当前 BMS 工作状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : None
//  Others     : 根据 BMS_STATUS_LED_MODE 选择 1 灯、2 灯或 4 灯识别逻辑
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_status_check(battery_state_t state);


/*******************************************************************************
//  Function   : bms_status_GetStatus
//  Description: 获取当前 BMS 状态识别结果
//  Param      : None
//  Return     : 指向内部 bms_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_status_t *bms_status_GetStatus(void);


#endif /* _BMS_STATUS_H_ */
