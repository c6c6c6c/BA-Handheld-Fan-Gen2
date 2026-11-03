
/**
 ***********************************************************************************************************************
 * @file    bsp_gpio.h
 * @brief   GPIO 底层驱动接口及板级引脚定义，实现 GPIO 模式配置及低功耗状态管理
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BSP_GPIO_H_
#define _BSP_GPIO_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 配置 GPIO 为通用输出模式
 * @param PORT GPIO 端口，例如 GPIOA、GPIOB、GPIOF
 * @param PIN  GPIO 引脚，例如 LL_GPIO_PIN_0
 * @param TYPE GPIO 输出类型：
 *             LL_GPIO_OUTPUT_PUSHPULL  - 推挽输出
 *             LL_GPIO_OUTPUT_OPENDRAIN - 开漏输出
 * @note  将指定引脚配置为通用输出模式；
 *        根据 TYPE 设置输出驱动类型；
 *        输出速度固定设置为 VERY_HIGH；
 *        本宏不主动设置 GPIO 输出电平。
 */
#define SET_GPIO_OUTPUT_MODE(PORT, PIN, TYPE)                    \
do                                                               \
{                                                                \
    LL_GPIO_SetPinMode(PORT, PIN, LL_GPIO_MODE_OUTPUT);          \
    LL_GPIO_SetPinOutputType(PORT, PIN, TYPE);                   \
    LL_GPIO_SetPinSpeed(PORT, PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);\
} while (0)

/**
 * @brief 配置 GPIO 为通用输入模式
 * @param PORT GPIO 端口，例如 GPIOA、GPIOB、GPIOF
 * @param PIN  GPIO 引脚，例如 LL_GPIO_PIN_0
 * @param PULL GPIO 内部上下拉配置：
 *             LL_GPIO_PULL_NO   - 无上下拉
 *             LL_GPIO_PULL_UP   - 内部上拉
 *             LL_GPIO_PULL_DOWN - 内部下拉
 * @note  将指定引脚配置为通用输入模式；
 *        根据 PULL 设置内部上下拉电阻；
 *        输入电平由外部电路及上下拉配置共同决定。
 */
#define SET_GPIO_INPUT_MODE(PORT, PIN, PULL)                     \
do                                                               \
{                                                                \
    LL_GPIO_SetPinMode(PORT, PIN, LL_GPIO_MODE_INPUT);           \
    LL_GPIO_SetPinPull(PORT, PIN, PULL);                         \
} while (0)


/* Button ------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风机按键 GPIO 定义
 * @note  FAN_SW 使用 PA8。
 *        正常工作时配置为上拉输入，
 *        用于检测风机按键状态。
 */
#define FAN_SW_PORT                        GPIOA
#define FAN_SW_PIN                         LL_GPIO_PIN_8

/**
 * @brief 雾化按键 GPIO 定义
 * @note  MIST_SW 使用 PB2。
 *        正常工作时配置为上拉输入，
 *        用于检测雾化按键状态。
 */
#define MIST_SW_PORT                       GPIOB
#define MIST_SW_PIN                        LL_GPIO_PIN_2

#ifdef POWER_CTRL_GPIO
/**
 * @brief 电源按键 GPIO 定义
 * @note  POWER_SW 使用 PA7。
 *        正常工作时配置为上拉输入；
 *        低功耗状态下可配合 EXTI Line7 实现唤醒；
 *        仅在 POWER_CTRL_GPIO 宏使能时参与编译。
 */
#define POWER_SW_PORT                      GPIOA
#define POWER_SW_PIN                       LL_GPIO_PIN_7
#endif


/* Display -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief AiP650E 显示驱动时钟 GPIO
 * @note  DISPLAY_CLK 使用 PB7。
 *        配置为开漏输出模式；
 *        用于输出显示芯片通信时钟信号；
 *        输出高电平时释放总线，由外部上拉提供高电平。
 */
#define DISPLAY_CLK_PORT                   GPIOB
#define DISPLAY_CLK_PIN                    LL_GPIO_PIN_7

/**
 * @brief AiP650E 显示驱动数据 GPIO
 * @note  DISPLAY_DIO 使用 PA9。
 *        配置为开漏输出模式；
 *        用于显示芯片数据通信；
 *        通过输出低电平或释放总线控制数据线状态。
 */
#define DISPLAY_DIO_PORT                   GPIOA
#define DISPLAY_DIO_PIN                    LL_GPIO_PIN_9


/* BMS Control -------------------------------------------------------------------------------------------------------*/

/**
 * @brief BMS 模拟按键及唤醒控制 GPIO
 * @note  BMS_KEY 使用 PF3。
 *        配置为推挽输出模式；
 *        默认输出低电平，表示释放模拟按键；
 *        根据现有硬件控制电路，输出高电平用于触发 KEY。
 */
#define BMS_KEY_PORT                       GPIOF
#define BMS_KEY_PIN                        LL_GPIO_PIN_3

/**
 * @brief BMS 控制 GPIO
 * @note  BMS_CTL 使用 PA2。
 *        配置为推挽输出模式；
 *        默认输出低电平；
 *        具体控制功能及有效电平由外部电路决定。
 */
#define BMS_CTL_PORT                       GPIOA
#define BMS_CTL_PIN                        LL_GPIO_PIN_2

/**
 * @brief BMS VIN 输入通路使能 GPIO
 * @note  BMS_VIN_EN 使用 PB5。
 *        配置为推挽输出模式；
 *        高电平使能 VIN 输入通路；
 *        低电平关闭 VIN 输入通路；
 *        系统初始化时默认输出高电平。
 */
#define BMS_VIN_EN_PORT                    GPIOB
#define BMS_VIN_EN_PIN                     LL_GPIO_PIN_5

/**
 * @brief BMS 电池通路使能 GPIO
 * @note  BMS_VBAT_EN 使用 PB4。
 *        配置为推挽输出模式；
 *        高电平使能电池通路；
 *        低电平关闭电池通路；
 *        系统初始化时默认输出高电平。
 */
#define BMS_VBAT_EN_PORT                   GPIOB
#define BMS_VBAT_EN_PIN                    LL_GPIO_PIN_4

/**
 * @brief BMS LED1 状态检测 GPIO
 * @note  BMS_LED1 使用 PF0。
 *        配置为无上下拉输入模式；
 *        用于读取电源管理芯片 LED1 状态信号；
 *        可结合电平变化及闪烁频率判断 BMS 状态。
 */
#define BMS_LED1_PORT                      GPIOF
#define BMS_LED1_PIN                       LL_GPIO_PIN_0

/**
 * @brief BMS LED2 状态检测 GPIO
 * @note  BMS_LED2 使用 PF1。
 *        配置为无上下拉输入模式；
 *        用于读取第二路 BMS LED 状态信号。
 *
 *        注意：此处保留原有 BMS_LED2 宏名称。
 *        若硬件实际连接 IP5326 LED3，应在硬件配置中明确映射。
 */
#define BMS_LED2_PORT                      GPIOF
#define BMS_LED2_PIN                       LL_GPIO_PIN_1

/**
 * @brief BMS USB 输入检测 GPIO
 * @note  BMS_USB_IN 使用 PA10。
 *        配置为无上下拉输入模式；
 *        用于读取 USB 输入状态；
 *        可结合 EXTI Line10 实现 USB 插入事件检测及低功耗唤醒。
 */
#define BMS_USB_IN_PORT                    GPIOA
#define BMS_USB_IN_PIN                     LL_GPIO_PIN_10


/* Standby -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 系统待机控制 GPIO
 * @note  STANDBY 使用 PB8。
 *        配置为推挽输出模式；
 *        系统正常运行时默认输出低电平；
 *        进入低功耗状态时设置为高电平；
 *        用于控制外部待机相关电路。
 */
#define STANDBY_PORT                       GPIOB
#define STANDBY_PIN                        LL_GPIO_PIN_8


/* Fan Control -------------------------------------------------------------------------------------------------------*/

/**
 * @brief 风机 GND 通路控制 GPIO
 * @note  FAN_CTRL 使用 PA4。
 *        配置为推挽输出模式；
 *        系统初始化时默认输出低电平；
 *        用于控制风机 GND 连接路径；
 *        实际导通条件由外部控制电路决定。
 */
#define FAN_CTRL_PORT                      GPIOA
#define FAN_CTRL_PIN                       LL_GPIO_PIN_4


/* Typedef -----------------------------------------------------------------------------------------------------------*/


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_gpio_Init
//  Description: 初始化板级 GPIO 引脚及默认工作状态
//  Param      : None
//  Return     : None
//  Others     : 使能 GPIOA、GPIOB、GPIOF 外设时钟；
//               初始化按键、显示、BMS、待机及风机控制 GPIO；
//               配置默认输入上下拉、输出模式及输出电平
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void bsp_gpio_Init(void);


/*******************************************************************************
//  Function   : gpio_sleep_open
//  Description: 配置系统进入低功耗状态前的 GPIO 工作状态
//  Param      : None
//  Return     : None
//  Others     : 调整按键输入模式及上下拉配置；
//               将显示通信引脚切换为高阻输入模式；
//               配置 BMS 电源通路及系统待机控制信号；
//               降低系统静态功耗及外设反向供电风险
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void gpio_sleep_open(void);


/*******************************************************************************
//  Function   : gpio_sleep_close
//  Description: 系统退出低功耗状态后恢复 GPIO 正常工作配置
//  Param      : None
//  Return     : None
//  Others     : 重新调用 bsp_gpio_Init()；
//               恢复各 GPIO 的默认工作模式、上下拉及输出电平；
//               适用于低功耗唤醒后的外设引脚重新初始化
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void gpio_sleep_close(void);


#endif /* _BSP_GPIO_H_ */
