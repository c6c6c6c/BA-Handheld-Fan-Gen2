/**
 ***********************************************************************************************************************
 * @file    timer_base.c
 * @brief
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "timer_base.h"

/* Variables ---------------------------------------------------------------------------------------------------------*/
__IO uint32_t SystemMs = 0;

/*******************************************************************************
* Function Name  : HAL_TIM_PeriodElapsedCallback
* Description    : 定时器中断处理回调
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void SystemMsUpdate(void)
{
	SystemMs++;
}

/*******************************************************************************
* Function Name  : getSystemMs
* Description    : 获取系统时间
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
uint32_t getSystemMs(void)
{
	return  SystemMs;
}

/*******************************************************************************
* Function Name  : timer_base_Timing
* Description    : 软件定时模式
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
bool timer_base_Timing(timer_base_t *tim, uint32_t timeout)
{
    uint32_t now = getSystemMs();
    if (tim->Init == 0U)
    {	
		tim->Init     = 1;
        tim->CurrTime = now;
        return false;
    }

    if ((uint32_t)(now - tim->CurrTime) >= timeout)
    {
        tim->CurrTime = getSystemMs();
        return true;
    }

    return false;
}

/*******************************************************************************
* Function Name  : timer_base_Count
* Description    : 软件计数模式
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
uint32_t timer_base_Count(timer_base_t *tim)
{
	if(tim->Init == 0)
	{
		tim->CurrTime = getSystemMs();
		tim->Init     = 1;
		return 0;
	}
	if(getSystemMs() > tim->CurrTime)
	{
		return getSystemMs() - tim->CurrTime;
	}
	
	return UINT32_MAX - tim->CurrTime + getSystemMs();
}

/*******************************************************************************
* Function Name  : timer_base_Rst
* Description    : 软件定时复位
* Input          : None
* Output         : None
* Return         : None
*******************************************************************************/
void timer_base_Rst(timer_base_t *tim)
{
	tim->Init = 0;
}
