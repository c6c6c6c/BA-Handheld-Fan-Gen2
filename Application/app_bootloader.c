/**
 ***********************************************************************************************************************
 * @file    app_bootloader.c
 * @brief   系统初始化、周期调度及风机/按键/LED 业务控制
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "app_bootloader.h"

#include "param.h"
#include "bms_bootloader.h"
#include "timer_base.h"

#include "bsp_bms_usart.h"
#include "bsp_flash.h"

#include "string.h"
/* Macro -------------------------------------------------------------------------------------------------------------*/


/* Variables ---------------------------------------------------------------------------------------------------------*/
flash_write_t flash_loading;

/* External variables ------------------------------------------------------------------------------------------------*/

void bootloader_ProgrammingInit(void);
/* Function implementations ------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : bootloader_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 按当前程序顺序依次初始化功能模块和底层硬件
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
static void JumpToApplication(void)
{
    func_pointer_t jump = (func_pointer_t)(*(__IO uint32_t *)(APP_BASE_ADDRESS + 4));

    __enable_irq();
    __set_MSP(*(__IO uint32_t *)(APP_BASE_ADDRESS));
    jump();
}


/*******************************************************************************
//  Function   : bootloader_init
//  Description: 执行系统整体初始化
//  Param      : None
//  Return     : None
//  Others     : 按当前程序顺序依次初始化功能模块和底层硬件
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
extern serial_data_t TxData;
void bootloader_init(void)
{    
    if (param_ota_state() == OTC_STATUS_SUCCESS)
    {
        JumpToApplication();
    }
	else
	{
		bootloader_ProgrammingInit();
	}
	bsp_bms_usart_Init();
	SerialCommuncaitionInit();
	TxData.Command = CMD_UI_OTC_PACK_ACK;
	
}
/*******************************************************************************
//  Function   : bootloader_run
//  Description: 执行系统主循环调度任务
//  Param      : None
//  Return     : None	
//  Others     : 每次调用处理通信和 Modbus；按 1ms、10ms 周期调度业务任务
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bootloader_run(void)
{
	static timer_base_t _time = {0};
	SerialRxDataHandler();
	SerialTxDataHandler();
	
	switch((uint8_t)param_ota_state())
	{
		case OTC_STATUS_START:

		break;

		case OTC_STATUS_PROGRAMMING:
			bsp_flash_Unlock();
			timer_base_Rst(&_time);
		break;

		case OTC_STATUS_SUCCESS:
			bsp_flash_Lock();
			if(timer_base_Timing(&_time,50))
			{
				NVIC_SystemReset();
			}
		break;

		default:
		break;
	}
}


/*******************************************************************************
//  Function   : bootloader_run
//  Description: 执行系统主循环调度任务
//  Param      : None
//  Return     : None	
//  Others     : 每次调用处理通信和 Modbus；按 1ms、10ms 周期调度业务任务
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bootloader_ProgrammingInit(void)
{
    flash_loading.Address      = APP_BASE_ADDRESS;
    flash_loading.AppSize      = 0;
    flash_loading.PackageCount = 0;
    flash_loading.PackageSize  = 0;
	flash_loading.frist        = 1;
	param_ota_save(OTC_STATUS_PROGRAMMING);
}

/*******************************************************************************
//  Function   : bootloader_run
//  Description: 执行系统主循环调度任务
//  Param      : None
//  Return     : None	
//  Others     : 每次调用处理通信和 Modbus；按 1ms、10ms 周期调度业务任务
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
bool bootloader_FlashPageProgramming(uint8_t *buffer, uint8_t length)
{
	if(flash_loading.frist && 0)
	{
		flash_loading.frist = 0;
		return true;
	}
    memset(flash_loading.Buffer.Byte, 0xFF, FLASH_PAGE_SIZE);
    memcpy(flash_loading.Buffer.Byte, buffer, length);
    bsp_flash_PageErase(flash_loading.Address);
    bsp_flash_PageProgramming(flash_loading.Address, flash_loading.Buffer.Word);
    if (bsp_flash_PageVerify(flash_loading.Address, flash_loading.Buffer.Word, FLASH_PAGE_SIZE / 4) == false)
    {
        return false;
    }
    flash_loading.Address     += length;
    flash_loading.AppSize     += length;
    flash_loading.PackageSize  = length;
    flash_loading.PackageCount++;
    return true;
}
