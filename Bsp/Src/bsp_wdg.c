/**
 ***********************************************************************************************************************
 * @file    bsp_wdg.c
 * @brief   IWDG 独立看门狗底层驱动实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-12
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bsp_wdg.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/* Variables ---------------------------------------------------------------------------------------------------------*/

/* External variables ------------------------------------------------------------------------------------------------*/

/* Constant ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_wdg_Init
//  Description: 初始化并启动 IWDG 独立看门狗
//  Param      : None
//  Return     : None
//  Others     : 使用 LSI，标称超时为 3 s；初始化完成后需在超时前定期喂狗；
//               超时由硬件复位 MCU，IWDG 启动后无法由软件停止
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_wdg_Init(void)
{
  /* PWR 时钟 + 后备域写保护(IWDG 时钟源选择寄存器在备份域) */
  LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
  LL_PWR_EnableBkUpAccess();
  while (LL_PWR_IsEnabledBkUpAccess() == 0)
  {
  }

  /* 使能 LSI */
  LL_RCC_LSI_Enable();
  while (LL_RCC_LSI_IsReady() == 0)
  {
  }

  /* IWDG 时钟源选择 LSI */
  LL_RCC_SetIWDGClockSource(LL_RCC_IWDG_CLKSOURCE_LSI);
  LL_PWR_DisableBkUpAccess();

  /* 启动 IWDG */
  LL_IWDG_Enable(IWDG);
  LL_IWDG_EnableWriteAccess(IWDG);
  LL_IWDG_SetPrescaler(IWDG, BSP_WDG_PRESCALER);
  LL_IWDG_SetReloadCounter(IWDG, BSP_WDG_RELOAD);        /* (3071 + 1) * 32 / 32768 = 3 s */

  while (LL_IWDG_IsReady(IWDG) == 0)
  {
  }

  /* 首次喂狗 */
  LL_IWDG_ReloadCounter(IWDG);
}
/*******************************************************************************
//  Function   : bsp_wdg_Deinit
//  Description: 保留看门狗反初始化接口，不执行硬件操作
//  Param      : None
//  Return     : None
//  Others     : IWDG 启动后无法由软件停止，LSI 也被强制保持开启；
//               本函数不关闭看门狗、不改变超时配置，调用后仍需定期喂狗
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_wdg_Deinit(void)
{
    /* 硬件不支持运行期间关闭 IWDG；保留空操作，避免改变现有保护行为。 */
}

/*******************************************************************************
//  Function   : bsp_wdg_Refresh
//  Description: 重装 IWDG 计数器，刷新看门狗
//  Param      : None
//  Return     : None
//  Others     : 在系统正常运行路径中调用，间隔应小于实际超时时间并预留裕量
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_wdg_Refresh(void)
{
    LL_IWDG_ReloadCounter(IWDG);
}

