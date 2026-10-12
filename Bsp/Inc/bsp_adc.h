/**
 ***********************************************************************************************************************
 * @file    bsp_adc.h
 * @brief   ADC 底层驱动接口及采样通道定义
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __BSP_ADC_H
#define __BSP_ADC_H

#include "main.h"
#include "py32f032_ll_adc.h"
#include "py32f032_ll_dma.h"


/* Macro Ctrl --------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC 是否使用 DMA 搬运采样数据
 *
 * @value 0 不使用 DMA，ADC 数据通过中断或软件读取获取
 * @value 1 使用 DMA 自动搬运 ADC 转换结果
 *
 * @note 修改该宏后，ADC 初始化及数据获取逻辑需要与 DMA 配置保持一致。
 */
#define ADC_USE_DMA                            1


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC 逻辑采样通道枚举
 *
 * 用于应用层区分不同模拟量采样数据，
 * 枚举顺序通常与 ADC 数据缓存数组下标保持一致。
 */
typedef enum
{
    ADC_CH_MOTOR = 0,                     /**< 电机电流反馈采样 */
    ADC_CH_WH,                            /**< WH 模拟信号采样 */
    ADC_CH_SW,                            /**< 按键/开关模拟信号采样 */
    ADC_CH_USB,                           /**< USB 输入电压采样 */
    ADC_CH_NTC,                           /**< NTC 温度检测采样 */
    ADC_CH_PGA2,                          /**< 内部 OPA2/PGA2 输出采样 */
    ADC_CH_VREFINT,                       /**< 内部 基准源采样 */
    ADC_CH_NUM                            /**< ADC 逻辑通道总数 */
} adc_Channel_t;


/* Macro -------------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC 外部触发源
 *
 * 使用 TIM3 TRGO 触发 ADC 规则组转换，
 * 可用于实现固定周期 ADC 采样。
 */
#define BSP_ADC_TRIGGER_SOURCE                  LL_ADC_REG_TRIG_EXT_TIM3_TRGO


/* MOTOR ADC ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief 电机电流反馈 ADC 配置
 *
 * GPIO : PA5
 * ADC  : ADC_CHANNEL_5
 */
#define BSP_ADC_MOTOR_GPIO_PORT                 GPIOA
#define BSP_ADC_MOTOR_GPIO_PIN                  LL_GPIO_PIN_5
#define BSP_ADC_MOTOR_GPIO_CLOCK_EN()           LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)
#define BSP_ADC_MOTOR_CHANNEL                   LL_ADC_CHANNEL_5


/* WH ADC ------------------------------------------------------------------------------------------------------------*/

/**
 * @brief WH 模拟信号 ADC 配置
 *
 * GPIO : PA6
 * ADC  : ADC_CHANNEL_6
 */
#define BSP_ADC_WH_GPIO_PORT                    GPIOA
#define BSP_ADC_WH_GPIO_PIN                     LL_GPIO_PIN_6
#define BSP_ADC_WH_GPIO_CLOCK_EN()              LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)
#define BSP_ADC_WH_CHANNEL                      LL_ADC_CHANNEL_6


/* SW ADC ------------------------------------------------------------------------------------------------------------*/

/**
 * @brief 按键/开关模拟信号 ADC 配置
 *
 * GPIO : PA7
 * ADC  : ADC_CHANNEL_7
 */
#define BSP_ADC_SW_GPIO_PORT                    GPIOA
#define BSP_ADC_SW_GPIO_PIN                     LL_GPIO_PIN_7
#define BSP_ADC_SW_GPIO_CLOCK_EN()              LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA)
#define BSP_ADC_SW_CHANNEL                      LL_ADC_CHANNEL_7


/* USB ADC -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief USB 输入电压检测 ADC 配置
 *
 * GPIO : PB0
 * ADC  : ADC_CHANNEL_8
 *
 * 用于采集 USB/VIN 输入电压，
 * 实际电压值需根据外围分压电阻比例进行换算。
 */
#define BSP_ADC_USB_GPIO_PORT                   GPIOB
#define BSP_ADC_USB_GPIO_PIN                    LL_GPIO_PIN_0
#define BSP_ADC_USB_GPIO_CLOCK_EN()             LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB)
#define BSP_ADC_USB_CHANNEL                     LL_ADC_CHANNEL_8


/* NTC ADC -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief NTC 温度检测 ADC 配置
 *
 * GPIO : PB1
 * ADC  : ADC_CHANNEL_9
 *
 * 用于采集 NTC 分压电压，
 * 上层根据 ADC 值换算 NTC 电阻及对应温度。
 */
#define BSP_ADC_NTC_GPIO_PORT                   GPIOB
#define BSP_ADC_NTC_GPIO_PIN                    LL_GPIO_PIN_1
#define BSP_ADC_NTC_GPIO_CLOCK_EN()             LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB)
#define BSP_ADC_NTC_CHANNEL                     LL_ADC_CHANNEL_9


/* PGA2 ADC ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief 内部 OPA2/PGA2 输出 ADC 通道
 *
 * PGA2 为 MCU 内部模拟信号，因此无需配置外部 GPIO。
 */
#define BSP_ADC_PGA2_CHANNEL                    LL_ADC_CHANNEL_OPA2


/* VREFINT ADC ----------------------------------------------------------------------------------------------------------*/
/**
 * @brief 内部 VREFINT(参考源) 输出 ADC 通道
 *
 * VREFINT 为 MCU 内部模拟信号，因此无需配置外部 GPIO。
 */
#define BSP_ADC_VREFINT_CHANNEL                 LL_ADC_CHANNEL_VREFINT


/* Function ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_adc_Init
//  Description: 初始化 ADC 模块
//  Param      : None
//  Return     : None
//  Others     : 完成 ADC 时钟、GPIO、采样通道、触发源及中断/DMA相关配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Init(void);


/*******************************************************************************
//  Function   : bsp_adc_Deinit
//  Description: 反初始化 ADC 模块
//  Param      : None
//  Return     : None
//  Others     : 停止 ADC 转换，并关闭 ADC 中断、DMA、外设时钟及相关 GPIO 配置
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Deinit(void);


/*******************************************************************************
//  Function   : bsp_adc_Start
//  Description: 启动 ADC 采样
//  Param      : None
//  Return     : None
//  Others     : 根据 ADC_USE_DMA 配置启动 ADC 中断采样或 DMA 采样
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Start(void);


/*******************************************************************************
//  Function   : bsp_adc_Stop
//  Description: 停止 ADC 采样
//  Param      : None
//  Return     : None
//  Others     : 停止 ADC 转换，并关闭相关中断或 DMA 请求
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Stop(void);

extern __IO uint32_t ADCxConvertedDatas[ADC_CH_NUM];
#endif /* __BSP_ADC_H */
