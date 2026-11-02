
/**
 ***********************************************************************************************************************
 * @file    bootloader.c
 * @brief   Bootloader 固件升级管理，实现 Application 跳转、OTA 初始化及 Flash 固件烧录
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-11-02
 ***********************************************************************************************************************
 * @note
 *          1. 支持从 Bootloader 跳转至 Application 程序。
 *          2. 支持 OTA 固件升级状态初始化及持久化保存。
 *          3. 以 Flash 页为单位执行固件擦除、写入及校验。
 *          4. 固件写入成功后更新烧录地址、数据长度及数据包计数。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bootloader.h"

#include "bsp_flash.h"
#include "param.h"

#include <string.h>


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief Bootloader 固件升级控制数据
 * @note  保存当前 Flash 写入地址、固件大小、数据包计数、
 *        最近一次数据包长度及 Flash 页缓存。
 */
boot_update_t boot_cmd;


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : JumpToApplication
//  Description: 从 Bootloader 跳转至 Application 程序
//  Param      : None
//  Return     : None
//  Others     : 从 Application 向量表读取初始 MSP 和复位入口地址；
//               恢复全局中断使能，设置 MSP 后跳转至 Application；
//               调用前应确保 Application 固件有效且外设状态已妥善处理
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void JumpToApplication(void)
{
    /* 从 Application 中断向量表读取复位入口地址。 */
    func_pointer_t jump = (func_pointer_t)(*(__IO uint32_t *)(APP_BASE_ADDRESS + 4U));

    /* 使能全局中断。 */
    __enable_irq();

    /* 将 MSP 设置为 Application 向量表中的初始栈顶地址。 */
    __set_MSP(*(__IO uint32_t *)(APP_BASE_ADDRESS));

    /* 跳转至 Application 复位入口。 */
    jump();
}


/*******************************************************************************
//  Function   : bootloader_Init
//  Description: 初始化 Bootloader OTA 固件升级控制参数
//  Param      : None
//  Return     : None
//  Others     : 将固件烧录地址设置为 Application 起始地址；
//               清零固件长度、数据包计数及包长度；
//               使用 0xFF 初始化 Flash 页缓存；
//               将 OTA 状态保存为正在编程
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void bootloader_Init(void)
{
    /* 设置 Application 固件烧录起始地址。 */
    boot_cmd.addr = APP_BASE_ADDRESS;

    /* 清除固件升级统计信息。 */
    boot_cmd.size      = 0U;
    boot_cmd.count     = 0U;
    boot_cmd.pack_size = 0U;

    /* 初始化 Flash 页缓存，未使用区域填充 0xFF。 */
    memset(boot_cmd.buffer.byte,0xFF,sizeof(boot_cmd.buffer.byte));

    /* 更新 OTA 状态为固件编程中。 */
    param_ota_save(OTC_STATUS_PROGRAMMING);
}


/*******************************************************************************
//  Function   : bootloader_burn
//  Description: 将接收到的 OTA 固件数据烧录至 Application Flash
//  Param      : buffer - 待烧录固件数据缓冲区首地址
//               length - 当前固件数据包长度，单位 Byte
//  Return     : true  - 当前数据包烧录及校验成功
//               false - 参数无效、地址越界或 Flash 数据校验失败
//  Others     : 每次处理一个 Flash 页；
//               未使用的页缓存空间使用 0xFF 填充；
//               先擦除目标 Flash 页，再执行页编程；
//               编程完成后读取校验整页数据；
//               校验成功后更新烧录地址、累计固件长度及包计数；
//               失败时不更新固件烧录进度
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
bool bootloader_burn(const uint8_t *buffer, uint8_t length)
{
    /* 检查数据缓冲区、数据长度及 Flash 烧录地址。 */
    if ((buffer == NULL) ||
        (length == 0U) ||
        (length > FLASH_PAGE_SIZE) ||
        (boot_cmd.addr < APP_BASE_ADDRESS) ||
        (boot_cmd.addr > (APP_END_ADDRESS - FLASH_PAGE_SIZE)))
    {
        return false;
    }

    /* 清空 Flash 页缓存，未使用区域填充 0xFF。 */
    memset(boot_cmd.buffer.byte,0xFF,sizeof(boot_cmd.buffer.byte));

    /* 将当前固件数据包复制至 Flash 页缓存。 */
    memcpy(boot_cmd.buffer.byte, buffer, length);

    /* 解锁 Flash。 */
    bsp_flash_Unlock();

    /* 擦除目标 Application Flash 页。 */
    bsp_flash_PageErase(boot_cmd.addr);

    /* 将当前页缓存写入 Flash。 */
    bsp_flash_PageProgramming(boot_cmd.addr, boot_cmd.buffer.word);

    /* 锁定 Flash，防止意外修改。 */
    bsp_flash_Lock();

    /* 校验目标 Flash 页是否与待写入数据完全一致。 */
    if (!bsp_flash_PageVerify(boot_cmd.addr,boot_cmd.buffer.word,FLASH_PAGE_SIZE / sizeof(uint32_t)))
    {
        return false;
    }

    /* 移动至下一个 Flash 烧录页地址。 */
    boot_cmd.addr += FLASH_PAGE_SIZE;

    /* 累计实际烧录的固件数据长度。 */
    boot_cmd.size += length;

    /* 保存最近一次烧录的数据包长度。 */
    boot_cmd.pack_size = length;

    /* 累计成功烧录的数据包数量。 */
    boot_cmd.count++;

    return true;
}
