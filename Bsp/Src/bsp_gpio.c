
/**
 ***********************************************************************************************************************
 * @file    bsp_gpio.c
 * @brief   板级 GPIO 初始化及低功耗状态管理，实现外设引脚配置、休眠切换及唤醒恢复
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bsp_gpio.h"


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_gpio_Init
//  Description: 初始化板级 GPIO 引脚及默认工作状态
//  Param      : None
//  Return     : None
//  Others     : 使能 GPIOA、GPIOB、GPIOF 外设时钟；
//               配置按键、显示、BMS、待机及风机控制引脚；
//               输出引脚在切换为输出模式前先设置默认电平，
//               避免初始化过程中产生异常控制脉冲
//  Author     : Chen Liang 2026-08-02
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

    /* FAN_SW：上拉输入，用于检测风机按键状态。 */
    SET_GPIO_INPUT_MODE(FAN_SW_PORT, FAN_SW_PIN, LL_GPIO_PULL_UP);

    /* MIST_SW：上拉输入，用于检测雾化按键状态。 */
    SET_GPIO_INPUT_MODE(MIST_SW_PORT, MIST_SW_PIN, LL_GPIO_PULL_UP);

#ifdef POWER_CTRL_GPIO
    /*
     * POWER_SW：
     * 配置为上拉输入，用于检测电源控制按键状态。
     * 该引脚仅在 POWER_CTRL_GPIO 宏使能时参与初始化。
     */
    SET_GPIO_INPUT_MODE(POWER_SW_PORT, POWER_SW_PIN, LL_GPIO_PULL_UP);
#endif

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 显示驱动 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * DISPLAY_CLK：
     *
     * 1. 默认设置输出数据寄存器为高电平。
     * 2. 配置为开漏输出模式。
     * 3. 输出高电平时释放总线，由外部上拉提供高电平。
     * 4. 用于 AiP650E 显示驱动时钟信号。
     */
    LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN);
    SET_GPIO_OUTPUT_MODE(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_OUTPUT_OPENDRAIN);

    /*
     * DISPLAY_DIO：
     *
     * 1. 默认设置输出数据寄存器为高电平。
     * 2. 配置为开漏输出模式。
     * 3. 输出低电平时主动拉低总线。
     * 4. 输出高电平时释放总线。
     * 5. 用于 AiP650E 显示驱动数据通信。
     */
    LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN);
    SET_GPIO_OUTPUT_MODE(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_OUTPUT_OPENDRAIN);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* BMS 控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * BMS_KEY：
     *
     * BMS 模拟按键控制信号。
     * 初始化为推挽输出，默认输出低电平。
     * 避免 MCU 上电初始化时误触发电源管理芯片的 KEY 功能。
     */
    LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_KEY_PORT, BMS_KEY_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_CTL：
     *
     * BMS 外部控制信号。
     * 初始化为推挽输出，默认输出低电平。
     * 具体控制作用由外部电源管理电路决定。
     */
    LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_CTL_PORT, BMS_CTL_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_VIN_EN：
     *
     * BMS VIN 输入通路使能控制信号。
     * 初始化为推挽输出，默认输出高电平。
     * 根据当前硬件控制逻辑，高电平用于使能 VIN 输入通路。
     */
    LL_GPIO_SetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_VBAT_EN：
     *
     * BMS 电池通路使能控制信号。
     * 初始化为推挽输出，默认输出高电平。
     * 根据当前硬件控制逻辑，高电平用于使能电池通路。
     */
    LL_GPIO_SetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);
    SET_GPIO_OUTPUT_MODE(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN, LL_GPIO_OUTPUT_PUSHPULL);

    /*
     * BMS_LED1：
     *
     * BMS 状态指示灯 LED1 电平检测输入。
     * 不启用 MCU 内部上下拉电阻。
     * 输入电平由外部 BMS 电路决定。
     */
    SET_GPIO_INPUT_MODE(BMS_LED1_PORT, BMS_LED1_PIN, LL_GPIO_PULL_NO);

    /*
     * BMS_LED2：
     *
     * BMS 状态指示灯 LED2 电平检测输入。
     * 不启用 MCU 内部上下拉电阻。
     * 可通过软件检测电平及闪烁状态。
     */
    SET_GPIO_INPUT_MODE(BMS_LED2_PORT, BMS_LED2_PIN, LL_GPIO_PULL_NO);

    /*
     * BMS_USB_IN：
     *
     * USB 输入状态检测信号。
     * 配置为无上下拉输入模式。
     * 可配合 EXTI 实现 USB 插入事件检测及低功耗唤醒。
     */
    SET_GPIO_INPUT_MODE(BMS_USB_IN_PORT, BMS_USB_IN_PIN, LL_GPIO_PULL_NO);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 待机控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * STANDBY：
     *
     * 系统待机控制输出信号。
     * 初始化为推挽输出，默认输出低电平。
     * 进入低功耗状态时由 gpio_sleep_open() 修改输出电平。
     */
    LL_GPIO_ResetOutputPin(STANDBY_PORT, STANDBY_PIN);
    SET_GPIO_OUTPUT_MODE(STANDBY_PORT, STANDBY_PIN, LL_GPIO_OUTPUT_PUSHPULL);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 风机控制 GPIO                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * FAN_CTRL：
     *
     * 风机 GND 通路控制信号。
     * 初始化为推挽输出，默认输出低电平。
     * 具体导通状态由外部 MOS 管及硬件电路决定。
     */
    LL_GPIO_ResetOutputPin(FAN_CTRL_PORT, FAN_CTRL_PIN);
    SET_GPIO_OUTPUT_MODE(FAN_CTRL_PORT, FAN_CTRL_PIN, LL_GPIO_OUTPUT_PUSHPULL);
}


/*******************************************************************************
//  Function   : gpio_sleep_open
//  Description: 配置系统进入低功耗状态前的 GPIO 工作状态
//  Param      : None
//  Return     : None
//  Others     : 调整按键输入上下拉及显示接口工作模式；
//               配置 BMS 电源通路、待机控制及风机控制信号；
//               降低系统静态功耗，并减少外设掉电后的反向灌电风险
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void gpio_sleep_open(void)
{
    /* -------------------------------------------------------------------------------------------------------------- */
    /* 按键低功耗配置                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * FAN_SW ： 切换为下拉输入模式，降低休眠期间的引脚悬空风险。
	 * MIST_SW： 切换为下拉输入模式，降低休眠期间的引脚悬空风险。
	 * POWER_SW：切换为无上下拉输入模式。 保留电源按键外部电平检测能力，可配合 EXTI 配置实现低功耗唤醒。
     */
    SET_GPIO_INPUT_MODE(FAN_SW_PORT, FAN_SW_PIN, LL_GPIO_PULL_DOWN);
    SET_GPIO_INPUT_MODE(MIST_SW_PORT, MIST_SW_PIN, LL_GPIO_PULL_DOWN);

#ifdef POWER_CTRL_GPIO
    /*
     * POWER_SW：
     * 切换为无上下拉输入模式。
     * 保留电源按键外部电平检测能力，
     * 可配合 EXTI 配置实现低功耗唤醒。
     */
    SET_GPIO_INPUT_MODE(POWER_SW_PORT, POWER_SW_PIN, LL_GPIO_PULL_NO);
#endif


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 显示驱动接口低功耗配置                                                                                          */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * DISPLAY_CLK / DISPLAY_DIO：
     *
     * 1. 首先将 GPIO 输出数据寄存器设置为高电平。
     * 2. 释放开漏输出驱动。
     * 3. 然后切换为无上下拉输入模式。
     * 4. MCU 引脚进入高阻状态。
     *
     * 目的：
     * 减少显示模块掉电时通过 GPIO 信号线产生的反向供电风险，
     * 降低系统休眠电流。
     *
     * 注意：
     * 实际引脚电平仍可能受外部上拉电阻及显示模块供电影响。
     */
    SET_GPIO_INPUT_MODE(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_PULL_NO);
    SET_GPIO_INPUT_MODE(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_PULL_NO);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* BMS 低功耗状态配置                                                                                              */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * BMS_KEY：
     *
     * 输出低电平，释放 BMS 模拟按键控制信号。
     * 避免休眠期间持续触发 BMS KEY 功能。
     */
	LL_GPIO_ResetOutputPin(BMS_KEY_PORT, BMS_KEY_PIN);
	
    /*
     * BMS_CTL：
     *
     * 输出低电平，恢复 BMS 控制信号的默认状态。
     */
    LL_GPIO_ResetOutputPin(BMS_CTL_PORT, BMS_CTL_PIN);
	
    /*
     * BMS_VIN_EN：
     *
     * 输出高电平，保持 VIN 输入通路使能。
     * 允许电源管理电路在休眠期间维持 USB 输入路径。
     */
    LL_GPIO_ResetOutputPin(BMS_VIN_EN_PORT, BMS_VIN_EN_PIN);

    /*
     * BMS_VBAT_EN：
     *
     * 输出低电平，关闭电池通路使能。
     * 用于降低休眠期间相关电源路径的功耗。
     */
    LL_GPIO_ResetOutputPin(BMS_VBAT_EN_PORT, BMS_VBAT_EN_PIN);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 系统待机控制                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * STANDBY：
     *
     * 休眠状态下设置为高电平。
     * 用于控制外部待机相关电路。
     *
     * 根据原有调试记录：
     * 该状态下测得系统待机电流约为 210 uA。
     * 实际电流与外设状态及硬件配置有关。
     */
    LL_GPIO_SetOutputPin(STANDBY_PORT, STANDBY_PIN);
	
    /* -------------------------------------------------------------------------------------------------------------- */
    /* 风机低功耗控制                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * FAN_CTRL：
     *
     * 用于控制风机 GND 连接路径。
     * 当前代码实际再次将 STANDBY 引脚设置为高电平，
     * 尚未直接修改 FAN_CTRL 引脚。
     *
     * FAN_CTRL 的具体休眠控制状态需结合硬件设计确认。
     */
    LL_GPIO_SetOutputPin(STANDBY_PORT, STANDBY_PIN);
}


/*******************************************************************************
//  Function   : gpio_sleep_close
//  Description: 系统退出低功耗状态后恢复 GPIO 正常工作配置
//  Param      : None
//  Return     : None
//  Others     : 重新初始化板级 GPIO；
//               恢复按键、显示、BMS 及风机控制引脚的默认配置
//  Author     : Chen Liang 2026-08-02
*******************************************************************************/
void gpio_sleep_close(void)
{
    /*
     * 休眠期间部分 GPIO 已切换为输入或高阻模式。
     *
     * 退出低功耗状态后重新执行 GPIO 初始化，
     * 恢复各外设控制引脚的默认输出电平和工作模式。
     */
    bsp_gpio_Init();
}
