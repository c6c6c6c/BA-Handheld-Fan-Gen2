/**
 ***********************************************************************************************************************
 * @file    bsp_gpio.c
 * @brief   板级 GPIO 初始化及低功耗状态配置
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "bsp_gpio.h"


/*******************************************************************************
//  Function   : bsp_gpio_Init
//  Description: 初始化板级 GPIO，包括按键、显示驱动、BMS 控制及待机控制引脚
//  Param      : None
//  Return     : None
//  Others     : GPIO 输出引脚在切换为输出模式前，先设置默认输出电平，避免初始化瞬间产生异常脉冲
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bsp_gpio_Init(void)
{
    /* 使能 GPIOA、GPIOB、GPIOF 外设时钟。 */
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOF);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 按键 GPIO                                                                                                      */
    /* -------------------------------------------------------------------------------------------------------------- */

    /* 风扇按键：上拉输入，按键按下时由外部电路拉低。 */
    SET_GPIO_INPUT_MODE(FAN_SW_PORT, FAN_SW_PIN, LL_GPIO_PULL_UP);

    /* 喷雾按键：上拉输入，按键按下时由外部电路拉低。 */
    SET_GPIO_INPUT_MODE(MIST_SW_PORT, MIST_SW_PIN, LL_GPIO_PULL_UP);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 显示驱动 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * DISPLAY_CLK：
     * 默认释放总线为高电平，配置为开漏输出。
     * 开漏方式便于外部上拉，并避免多个器件驱动总线时发生电平冲突。
     */
    LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN);
    SET_GPIO_OUTPUT_MODE(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_OUTPUT_OPENDRAIN);

    /*
     * DISPLAY_DIO：
     * 默认释放数据线为高电平，配置为开漏输出。
     * 数据通信过程中可通过输出低电平或释放总线实现逻辑电平控制。
     */
    LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN);
    SET_GPIO_OUTPUT_MODE(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_OUTPUT_OPENDRAIN);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* BMS 控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * BMS_KEY：
     * BMS 按键/唤醒控制信号。
     * 初始化默认输出低电平，避免上电过程中误触发 BMS。
     */
    LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_KEY_PORT, BMS_KEY_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_CTL：
     * BMS 控制信号。
     * 初始化默认输出低电平。
     */
    LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_CTL_PORT, BMS_CTL_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_VIN_EN：
     * BMS VIN 输入通路使能信号。
     * 初始化输出高电平，保持 VIN 输入通路开启。
     */
    LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_VBAT_EN：
     * 电池供电/负载通路使能信号。
     * 初始化默认输出低电平。
     */
    LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_LED1：
     * BMS LED1 状态信号输入。
     * 不启用 MCU 内部上下拉，由外部 BMS 电路决定实际电平。
     */
    SET_GPIO_INPUT_MODE(BMS_LED1_PORT, BMS_LED1_PIN, LL_GPIO_PULL_NO);

    /*
     * BMS_LED2：
     * BMS LED2 状态信号输入。
     * 不启用 MCU 内部上下拉，由外部 BMS 电路决定实际电平。
     */
    SET_GPIO_INPUT_MODE(BMS_LED2_PORT, BMS_LED2_PIN, LL_GPIO_PULL_NO);

    /*
     * BMS_USB_IN：
     * BMS USB 唤醒信号。
     * 不启用 MCU 内部上下拉，由外部 BMS 电路决定实际电平。
     */
    SET_GPIO_INPUT_MODE(BMS_USB_IN_PORT, BMS_USB_IN_PIN, LL_GPIO_PULL_NO);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 待机控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * STANDBY：
     * 系统待机控制信号。
     * 正常工作状态下默认输出低电平。
     */
    LL_GPIO_ResetOutputPin(STANDBY_PORT, STANDBY_PIN);
    SET_GPIO_OUTPUT_MODE(STANDBY_PORT, STANDBY_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 风机控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * FAN CTRL：
     * 用于控制风机GND连接。
     * 正常工作状态下默认输出低电平。
     */
    LL_GPIO_ResetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN);
    SET_GPIO_OUTPUT_MODE(FAN_CTRL_PORT, FAN_CTRL_PIN, LL_GPIO_OUTPUT_PUSHPULL);
}


/*******************************************************************************
//  Function   : gpio_sleep_open
//  Description: 配置系统进入低功耗状态前的 GPIO 电平及工作模式
//  Param      : None
//  Return     : None
//  Others     : 主要用于降低静态功耗，并避免外设掉电后通过 GPIO 产生反向灌电
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void gpio_sleep_open(void)
{
    /* -------------------------------------------------------------------------------------------------------------- */
    /* 显示驱动接口低功耗处理                                                                                          */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 显示 CLK/DIO 为开漏总线。
     *
     * 进入休眠前：
     * 1. 先写高电平，使开漏输出 MOS 关闭，相当于释放总线。
     * 2. 再切换为无上下拉输入模式，使 MCU 对外呈高阻态。
     *
     * 目的：
     * 避免显示模块掉电后，MCU 通过显示接口向外设反向灌电，
     * 从而降低系统休眠电流。
     */
    LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN);
    LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN);
    SET_GPIO_INPUT_MODE(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_PULL_NO);
    SET_GPIO_INPUT_MODE(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_PULL_NO);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* BMS 低功耗状态配置                                                                                              */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * BMS_KEY：
     * 关闭瞬时按键/唤醒控制信号，防止休眠过程中误触发。
     */
    LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN);

    /*
     * BMS_CTL：
     * 关闭 BMS 相关控制输出。
     */
    LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN);

    /*
     * BMS_VIN_EN：
     * 保持 VIN 输入通路开启，
     * 使系统休眠状态下仍可以通过 USB/VIN 对电池充电。
     */
    LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);

    /*
     * BMS_VBAT_EN：
     * 关闭电池负载输出通路，降低系统休眠功耗。
     */
    LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* Standby 控制                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 休眠状态下将 STANDBY 设置为高电平。
     *
     * 根据实测，该状态可降低待机电流，
     * 待机电流约为 210 uA。
     */
    LL_GPIO_SetOutputPin(STANDBY_PORT, STANDBY_PIN);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 风机控制                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 休眠状态下将 FAN CTRL 设置为低电平。
     *
     * 风机GND连接将连接至弱地减小电流
     */
    LL_GPIO_SetOutputPin(STANDBY_PORT, STANDBY_PIN);
}


/*******************************************************************************
//  Function   : gpio_sleep_close
//  Description: 系统退出低功耗状态后恢复 GPIO 正常工作配置
//  Param      : None
//  Return     : None
//  Others     : 重新执行 GPIO 初始化，恢复各引脚默认模式及输出电平
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void gpio_sleep_close(void)
{
    /*
     * 休眠过程中部分 GPIO 已切换为输入高阻模式，
     * 因此唤醒后重新执行 GPIO 初始化，
     * 恢复显示、BMS 及待机控制相关 GPIO 的正常工作状态。
     */
    bsp_gpio_Init();
}
