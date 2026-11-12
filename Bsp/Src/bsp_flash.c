/**
 ***********************************************************************************************************************
 * @file    bsp_flash.c
 * @brief   Flash 页擦除、编程、读取、校验及芯片 UID 读取实现
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "bsp_flash.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/* Variables ---------------------------------------------------------------------------------------------------------*/

/* External variables ------------------------------------------------------------------------------------------------*/

/* Constant ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bsp_flash_Unlock
//  Description: 解锁 Flash 擦除及编程操作
//  Param      : None
//  Return     : None
//  Others     : 擦除或编程前调用，操作结束后应调用 bsp_flash_Lock()
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_Unlock(void)
{
    LL_FLASH_Unlock(FLASH);
}

/*******************************************************************************
//  Function   : bsp_flash_Lock
//  Description: 锁定 Flash 擦除及编程操作
//  Param      : None
//  Return     : None
//  Others     : 写操作结束后调用；锁定不影响存储器读取
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_Lock(void)
{
    LL_FLASH_Lock(FLASH);
}

/*******************************************************************************
//  Function   : bsp_flash_PageErase
//  Description: 擦除指定 Flash 页面
//  Param      : addr - 待擦除页面地址，应按 FLASH_PAGE_SIZE 对齐
//  Return     : None
//  Others     : 调用前需解锁 Flash；阻塞等待 BUSY/EOP，无超时及错误返回
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_PageErase(uint32_t addr)
{
    /* Wait Busy0=0 */
    while(LL_FLASH_IsActiveFlag_BUSY(FLASH)==1);

    /* Clear EOP */
    LL_FLASH_ClearFlag_EOP(FLASH);

    /* Enable Page Erase */
    LL_FLASH_EnablePageErase(FLASH);

    /* Set Erase Address */
    LL_FLASH_SetEraseAddress(FLASH,addr);

    /* Wait Busy0=0 */
    while(LL_FLASH_IsActiveFlag_BUSY(FLASH)==1);

    /* Wait EOP=1 */
    while(LL_FLASH_IsActiveFlag_EOP(FLASH)==0);

    /* Clear EOP */
    LL_FLASH_ClearFlag_EOP(FLASH);

    /* Disable Page Erase */
    LL_FLASH_DisablePageErase(FLASH);
}

/*******************************************************************************
//  Function   : bsp_flash_PageProgramming
//  Description: 编程一个 Flash 页面
//  Param      : addr - 目标页面起始地址，应按 FLASH_PAGE_SIZE 对齐
//               buffer - 源数据缓冲区，至少包含 32 个 uint32_t（128 字节）
//  Return     : None
//  Others     : 调用前需解锁并按需擦除页面；阻塞等待完成，无超时及错误返回，
//               函数结束时无条件使能全局中断
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_PageProgramming(uint32_t addr, uint32_t *buffer)
{
    /* Wait Busy=0 */
    while(LL_FLASH_IsActiveFlag_BUSY(FLASH)==1);

    /* Clear EOP */
    LL_FLASH_ClearFlag_EOP(FLASH);

    /* Enable Program */
    LL_FLASH_EnablePageProgram(FLASH);

    /* Page Program */
    LL_FLASH_PageProgram(FLASH,addr,buffer);
    
    /* Wait Busy0=0 */
    while(LL_FLASH_IsActiveFlag_BUSY(FLASH)==1);
    
    /* Wait EOP=1 */
    while(LL_FLASH_IsActiveFlag_EOP(FLASH)==0);
    
    /* Clear EOP */
    LL_FLASH_ClearFlag_EOP(FLASH);
   
    /* Disable Program */
    LL_FLASH_DisablePageProgram(FLASH);
}

/*******************************************************************************
//  Function   : bsp_flash_PageRead
//  Description: 读取一个 Flash 页面的数据
//  Param      : addr - 待读取页面起始地址，应按 4 字节对齐
//               buffer - 接收缓冲区，至少容纳 FLASH_PAGE_SIZE / 4 个 uint32_t
//  Return     : None
//  Others     : 按 32 位读取 FLASH_PAGE_SIZE 字节；调用方负责地址及缓冲区有效性
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_PageRead(uint32_t addr, uint32_t *buffer)
{
    /* 每次读取 4 字节，共读取一个完整页面。 */
    for (uint32_t i = 0; i < FLASH_PAGE_SIZE / 4; i++)
    {
        *buffer++ = *(__IO uint32_t *)(addr + (4U * i));
    }
}

/*******************************************************************************
//  Function   : bsp_flash_FlashBlank
//  Description: 比较 Flash 数据与缓冲区内容
//  Param      : addr - 待校验起始地址，应按 4 字节对齐
//               buffer - 参考数据缓冲区，至少包含 length 个 uint32_t
//               length - 校验长度，单位为 uint32_t 个数，不是字节数
//  Return     : true - 全部一致；false - 存在差异
//  Others     : 遇到首个不一致即返回 false；length 为 0 时返回 true
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
bool bsp_flash_FlashBlank(uint32_t addr)
{
    uint32_t word_index;

    for (word_index = 0U; word_index < (FLASH_PAGE_SIZE / sizeof(uint32_t)); word_index++)
    {
        if (*(volatile const uint32_t *)(addr + word_index * sizeof(uint32_t)) != 0xFFFFFFFFUL)
        {
            return false;
        }
    }
    return true;
}

/*******************************************************************************
//  Function   : bsp_flash_PageVerify
//  Description: 比较 Flash 数据与缓冲区内容
//  Param      : addr - 待校验起始地址，应按 4 字节对齐
//               buffer - 参考数据缓冲区，至少包含 length 个 uint32_t
//               length - 校验长度，单位为 uint32_t 个数，不是字节数
//  Return     : true - 全部一致；false - 存在差异
//  Others     : 遇到首个不一致即返回 false；length 为 0 时返回 true
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
bool bsp_flash_PageVerify(uint32_t addr, uint32_t *buffer, uint32_t length)
{
    /* length 按 32 位字计数，校验范围可以跨页。 */
    for (uint32_t i = 0; i < length; i++)
    {
        if (*(__IO uint32_t *)(addr + (4U * i)) != *buffer++)
        {
            return false;
        }
    }
    return true;
}

/*******************************************************************************
//  Function   : bsp_flash_WordRead
//  Description: 读取 Flash 中的一个 32 位字
//  Param      : addr - 待读取地址，应按 4 字节对齐
//  Return     : 指定地址处的 32 位数据
//  Others     : 直接读取存储器，调用方负责地址有效性
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t bsp_flash_WordRead(uint32_t addr)
{
    return (*(__IO uint32_t *)(addr));
}

/*******************************************************************************
//  Function   : bsp_flash_ReadUID
//  Description: 读取芯片 UID 区域数据
//  Param      : buffer - 接收缓冲区，至少容纳 3 个 uint32_t（12 字节）
//  Return     : None
//  Others     : PY32F032 的标准 UID 长度为 96 位
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_ReadUID(uint32_t *buffer)
{
    buffer[0] = *(__IO uint32_t *)(FLASH_UID_BASE + 0x00);
    buffer[1] = *(__IO uint32_t *)(FLASH_UID_BASE + 0x04);
    buffer[2] = *(__IO uint32_t *)(FLASH_UID_BASE + 0x08);
}
