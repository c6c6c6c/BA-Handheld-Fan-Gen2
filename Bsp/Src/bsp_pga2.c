/**
 ***********************************************************************************************************************
 * @file    bsp_pga2.c
 * @brief   OPA2/PGA2 底层驱动实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-10
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "bsp_pga2.h"



/* Private function --------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_pga2_io_config
//  Description: 初始化 PGA2 输入引脚
//  Param      : None
//  Return     : None
//  Others     : PB3 配置为 PGA2_INP，PA15 配置为 PGA2_INN，均使用模拟模式
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
static void bsp_pga2_io_config(void)
{
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    LL_GPIO_SetPinMode(GPIOB, LL_GPIO_PIN_3, LL_GPIO_MODE_ANALOG);
    LL_GPIO_SetPinMode(GPIOA, LL_GPIO_PIN_15, LL_GPIO_MODE_ANALOG);
}

/* Exported function -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_pga2_Init
//  Description: 初始化 OPA2/PGA2
//  Param      : None
//  Return     : None
//  Others     : PB3、PA15 分别作为 PGA2 正、负输入；OPA2 配置为 PGA 模式，
//               增益设置为 16 倍，偏置电压设置为 VCC/2；关闭外部输出后，
//               OPA2 输出通过内部通路映射至 ADC 的 LL_ADC_CHANNEL_OPA2
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_pga2_Init(void)
{
    bsp_pga2_io_config();

    /* 使能 OPA2 外设时钟，并在修改工作参数前保持 OPA2 关闭。 */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_OPA2);
    LL_OPA_Disable(OPA2);

    /* 配置 OPA2 为同相 PGA 模式，增益设置为 16 倍。 */
    LL_OPA_SetFunctionalMode(OPA2, LL_OPA_MODE_PGA);
    LL_OPA_SetPGAGain(OPA2, LL_OPA_PGA_GAIN_16);

    /*
     * 使能内部偏置并选择 VCC/2。
     * 当前厂商 LL_OPA_SetBiasVoltage() 固定访问 OPA1，不能用于 OPA2，
     * 因此在此直接配置 OPA2 的 OPA_VBIAS 位。
     */
    LL_OPA_EnableBiasSelection(OPA2);
    LL_OPA_SetBiasVoltage(OPA2,LL_OPA_BIASVOLTAGE_VCC_2);

    /*
     * OPA2 输出由芯片内部硬连线映射至 ADC 的 LL_ADC_CHANNEL_OPA2。
     * 关闭外部输出，避免占用 PB4/PGA2_OUT 引脚。
     */
    LL_OPA_DisableExternalOutput(OPA2);

    /* 完成全部参数配置后使能 OPA2。 */
    LL_OPA_Enable(OPA2);
}

/*******************************************************************************
//  Function   : bsp_pga2_Deinit
//  Description: 反初始化 OPA2/PGA2
//  Param      : None
//  Return     : None
//  Others     : 关闭 OPA2，复位 OPA2 外设寄存器并关闭外设时钟；PB3、PA15
//               配置为无上下拉模拟模式，以降低休眠状态下的引脚功耗
//  Author     : Chen Liang 2026-10-10
*******************************************************************************/
void bsp_pga2_Deinit(void)
{
    LL_OPA_Disable(OPA2);

    LL_APB1_GRP1_ForceReset(LL_APB1_GRP1_PERIPH_OPA2);
    LL_APB1_GRP1_ReleaseReset(LL_APB1_GRP1_PERIPH_OPA2);
    LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_OPA2);
}
