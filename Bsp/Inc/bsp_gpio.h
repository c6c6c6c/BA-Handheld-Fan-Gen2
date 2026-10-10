/**
 ***********************************************************************************************************************
 * @file    bsp_gpio.h
 * @brief   GPIO 底层驱动接口及引脚定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BSP_GPIO_H_
#define _BSP_GPIO_H_

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 设置 GPIO 为输出模式
 *
 * @param PORT GPIO 端口，例如 GPIOA
 * @param PIN  GPIO 引脚，例如 LL_GPIO_PIN_0
 * @param TYPE 输出类型，可选：
 *             - LL_GPIO_OUTPUT_PUSHPULL  推挽输出
 *             - LL_GPIO_OUTPUT_OPENDRAIN 开漏输出
 *
 * @note 默认输出速度设置为 VERY_HIGH。
 */
#define SET_GPIO_OUTPUT_MODE(PORT, PIN, TYPE)                    \
do                                                               \
{                                                                \
    LL_GPIO_SetPinMode(PORT, PIN, LL_GPIO_MODE_OUTPUT);          \
    LL_GPIO_SetPinOutputType(PORT, PIN, TYPE);                   \
    LL_GPIO_SetPinSpeed(PORT, PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);\
} while (0)

/**
 * @brief 设置 GPIO 为输入模式
 *
 * @param PORT GPIO 端口，例如 GPIOA
 * @param PIN  GPIO 引脚，例如 LL_GPIO_PIN_0
 * @param PULL 上下拉配置，可选：
 *             - LL_GPIO_PULL_NO   无上下拉
 *             - LL_GPIO_PULL_UP   上拉
 *             - LL_GPIO_PULL_DOWN 下拉
 */
#define SET_GPIO_INPUT_MODE(PORT, PIN, PULL)                     \
do                                                               \
{                                                                \
    LL_GPIO_SetPinMode(PORT, PIN, LL_GPIO_MODE_INPUT);           \
    LL_GPIO_SetPinPull(PORT, PIN, PULL);                         \
} while (0)


/* Button ------------------------------------------------------------------------------------------------------------*/

/** @brief 风扇按键 GPIO */
#define FAN_SW_PORT                        GPIOA
#define FAN_SW_PIN                         LL_GPIO_PIN_8

/** @brief 喷雾按键 GPIO */
#define MIST_SW_PORT                       GPIOA
#define MIST_SW_PIN                        LL_GPIO_PIN_9


/* Display -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 显示驱动时钟引脚
 *
 * 用于输出显示芯片通信时钟信号。
 */
#define DISPLAY_CLK_PORT                   GPIOB
#define DISPLAY_CLK_PIN                    LL_GPIO_PIN_7

/**
 * @brief 显示驱动数据引脚
 *
 * 用于显示芯片双向/单向数据通信。
 */
#define DISPLAY_DIO_PORT                   GPIOA
#define DISPLAY_DIO_PIN                    LL_GPIO_PIN_10


/* BMS Control -------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 按键/唤醒控制引脚
 *
 * 用于模拟按键或触发 BMS 唤醒。
 */
#define BMS_KEY_PORT                       GPIOF
#define BMS_KEY_PIN                        LL_GPIO_PIN_3

/**
 * @brief BMS 主控制引脚
 *
 * 用于控制 BMS 相关功能或工作状态。
 */
#define BMS_CTL_PORT                       GPIOA
#define BMS_CTL_PIN                        LL_GPIO_PIN_2

/**
 * @brief BMS VIN 输入通路使能引脚
 *
 * 用于控制 VIN 输入路径的开启和关闭。
 */
#define BMS_VIN_EN_PORT                    GPIOB
#define BMS_VIN_EN_PIN                     LL_GPIO_PIN_6

/**
 * @brief BMS VBAT 电池通路使能引脚
 *
 * 用于控制电池供电路径的开启和关闭。
 */
#define BMS_VBAT_EN_PORT                   GPIOB
#define BMS_VBAT_EN_PIN                    LL_GPIO_PIN_5

/**
 * @brief BMS LED1 状态信号输入引脚
 *
 * 用于读取 BMS LED1 状态。
 */
#define BMS_LED1_PORT                      GPIOF
#define BMS_LED1_PIN                       LL_GPIO_PIN_0

/**
 * @brief BMS LED3 状态信号输入引脚
 *
 * 用于读取 BMS LED3 状态。
 */
#define BMS_LED3_PORT                      GPIOF
#define BMS_LED3_PIN                       LL_GPIO_PIN_1

/**
 * @brief BMS USB 插入 状态信号输入引脚
 *
 * 用于读取 BMS USB 插入 状态。
 */
#define BMS_USB_IN_PORT                    GPIOA
#define BMS_USB_IN_PIN                     LL_GPIO_PIN_10


/* Standby -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 系统待机控制引脚
 *
 * 用于控制外部电路进入或退出待机状态。
 */
#define STANDBY_PORT                       GPIOB
#define STANDBY_PIN                        LL_GPIO_PIN_8


/* Fan Control -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风机控制引脚
 *
 * 用于控制风机GND连接。
 */
#define FAN_CTRL_PORT                     GPIOA
#define FAN_CTRL_PIN                      LL_GPIO_PIN_4


/* Typedef -----------------------------------------------------------------------------------------------------------*/


/* Function declarations ---------------------------------------------------------------------------------------------*/

/**
 * @brief 初始化板级 GPIO
 *
 * 配置按键、显示、BMS 控制以及待机相关 GPIO。
 */
void bsp_gpio_Init(void);

/**
 * @brief 配置 GPIO 进入低功耗状态
 *
 * 在系统进入 Sleep/Stop 等低功耗模式前调用，
 * 根据硬件需求调整 GPIO 状态以降低功耗。
 */
void gpio_sleep_open(void);

/**
 * @brief 恢复 GPIO 正常工作状态
 *
 * 系统退出低功耗模式后调用，
 * 恢复 GPIO 至正常运行配置。
 */
void gpio_sleep_close(void);

#endif /* _BSP_GPIO_H_ */
