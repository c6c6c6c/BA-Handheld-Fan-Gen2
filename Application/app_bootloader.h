/**
 ***********************************************************************************************************************
 * @file    app_bootloader.h
 * @brief   系统运行状态、故障状态及系统任务接口定义
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _APP_BOOTLOADER_H_
#define _APP_BOOTLOADER_H_

#include "main.h"


#define MCU_BASE_ADDRESS   0x8000000


#define APP_BASE_ADDRESS   (MCU_BASE_ADDRESS + 0x4000)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

typedef struct
{
    uint32_t Address;
    uint32_t AppSize;
    uint16_t PackageCount;
    uint16_t PackageSize;
	uint16_t frist;
    union
    {
        uint8_t  Byte[FLASH_PAGE_SIZE];
        uint32_t Word[FLASH_PAGE_SIZE / 4];
    } Buffer;
} flash_write_t;

typedef void (*func_pointer_t)(void);

/* Function declarations ---------------------------------------------------------------------------------------------*/
void bootloader_ProgrammingInit(void);
bool bootloader_FlashPageProgramming(uint8_t *buffer, uint8_t length);

/*******************************************************************************
//  Function   : system_init
//  Description: 初始化系统底层驱动、功能模块及通信模块
//  Param      : None
//  Return     : None
//  Others     : 系统启动时调用一次
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bootloader_init(void);

/*******************************************************************************
//  Function   : system_run
//  Description: 执行系统主循环任务
//  Param      : None
//  Return     : None
//  Others     : 需要在主循环中持续调用
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void bootloader_run(void);

#endif

