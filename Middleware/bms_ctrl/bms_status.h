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

/* Macro -------------------------------------------------------------------------------------------------------------*/

/** IP5326 单灯模式 */
#define BMS_STATUS_MODE_1_LED       1U

/** IP5326 双灯模式 */
#define BMS_STATUS_MODE_2_LED       2U

/** IP5326 四灯模式 */
#define BMS_STATUS_MODE_4_LED       4U


/** 默认使用双灯状态识别模式 */
#ifndef BMS_STATUS_LED_MODE
#define BMS_STATUS_LED_MODE         BMS_STATUS_MODE_2_LED
#endif


/* LED GPIO 状态读取 -------------------------------------------------------------------------------------------------*/

#define BMS_IO_LED1_IS_HIGH()       (LL_GPIO_IsInputPinSet(BMS_LED1_PORT, BMS_LED1_PIN) != 0U)

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    #define BMS_IO_LED2_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED2_PORT, BMS_LED2_PIN) != 0U)

#elif (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    #define BMS_IO_LED3_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED3_PORT, BMS_LED3_PIN) != 0U)
    #define BMS_IO_LED4_IS_HIGH()   (LL_GPIO_IsInputPinSet(BMS_LED4_PORT, BMS_LED4_PIN) != 0U)

#endif


/* LED pin check ------------------------------------------------------------------------------------------------------*/

/** 单灯模式必须配置 LED1 GPIO */
#if BMS_MODULE_STATUS_ENABLE && (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_1_LED)

    #if !defined(BMS_LED1_PORT) || !defined(BMS_LED1_PIN)
        #error "1-LED mode requires BMS_LED1 pin mappings in bsp_gpio.h"
    #endif

#endif


/** 双灯模式必须配置对应的两路 LED GPIO */
#if BMS_MODULE_STATUS_ENABLE && (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED)

    #if !defined(BMS_LED1_PORT) || !defined(BMS_LED1_PIN) || \
        !defined(BMS_LED2_PORT) || !defined(BMS_LED2_PIN)
        #error "2-LED mode requires BMS_LED1/LED2 pin mappings in bsp_gpio.h"
    #endif

#endif


/** 四灯模式必须完整配置 LED1~LED4 GPIO */
#if BMS_MODULE_STATUS_ENABLE && (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)

    #if !defined(BMS_LED1_PORT) || !defined(BMS_LED1_PIN) || \
        !defined(BMS_LED2_PORT) || !defined(BMS_LED2_PIN) || \
        !defined(BMS_LED3_PORT) || !defined(BMS_LED3_PIN) || \
        !defined(BMS_LED4_PORT) || !defined(BMS_LED4_PIN)
        #error "4-LED mode requires BMS_LED1/LED2/BMS_LED3/LED4 pin mappings in bsp_gpio.h"
    #endif

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
    BMS_STATUS_UNKNOWN = 0,     /**< 状态未知或 LED 组合不符合当前模式 */
    BMS_STATUS_CHARGING,        /**< 充电中 */
    BMS_STATUS_FULL,            /**< 充电完成 */
    BMS_STATUS_DISCHARGING,     /**< 正常放电 */
    BMS_STATUS_LOW_BATTERY      /**< 低电量放电 */
} bms_status_mode_t;


/**
 * @brief LED 闪烁检测数据
 */
typedef struct
{
    uint8_t  last_level;        /**< 上一次采样电平 */
    uint32_t last_edge_tick;    /**< 上一次电平跳变时刻，单位 ms */
    uint32_t last_period_ms;    /**< 最近一次检测到的闪烁周期，单位 ms */
    uint8_t  edge_count;        /**< 已检测到的有效边沿数量 */
} led_detect_t;


/**
 * @brief 单路 LED 状态数据
 */
typedef struct
{
    led_state_t  state;         /**< 当前 LED 识别状态 */
    led_detect_t detect;        /**< LED 闪烁检测数据 */
} led_dat;


/**
 * @brief BMS LED 状态识别结果
 */
typedef struct
{
    led_dat led1;               /**< LED1 状态 */

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_2_LED) || \
    (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)
    led_dat led2;               /**< LED2 状态 */
#endif

#if (BMS_STATUS_LED_MODE == BMS_STATUS_MODE_4_LED)
    led_dat led3;               /**< LED3 状态 */
    led_dat led4;               /**< LED4 状态 */
#endif

    bms_status_mode_t mode;     /**< 根据 LED 组合识别出的当前 BMS 状态 */
} bms_status_t;


/* Function declarations ---------------------------------------------------------------------------------------------*/

#if BMS_MODULE_STATUS_ENABLE

/*******************************************************************************
//  Function   : bms_status_Init
//  Description: 初始化 BMS LED 状态识别模块
//  Param      : None
//  Return     : true：初始化成功；false：LED 输入 GPIO 配置错误
//  Others     : 仅初始化状态识别，不控制 VIN/VBAT、KEY 或保护模块
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_status_Init(void);


/*******************************************************************************
//  Function   : bms_status_check
//  Description: 更新 LED 状态并识别当前 BMS 工作状态
//  Param      : IsCharge - 非 0：充电；0：放电
//  Return     : None
//  Others     : 根据 BMS_STATUS_LED_MODE 选择 1 灯、2 灯或 4 灯识别逻辑
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_status_check(uint8_t IsCharge);


/*******************************************************************************
//  Function   : bms_status_GetStatus
//  Description: 获取当前 BMS 状态识别结果
//  Param      : None
//  Return     : 指向内部 bms_status_t 状态结构体的只读指针
//  Others     : 返回对象由模块内部维护，调用方不应修改
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_status_t *bms_status_GetStatus(void);

#endif /* BMS_MODULE_STATUS_ENABLE */


#endif /* _BMS_STATUS_H_ */
