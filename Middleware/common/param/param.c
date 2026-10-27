/**
 ***********************************************************************************************************************
 * @file    param.c
 * @brief   Flash参数管理模块，用于按键记录、故障记录、产品信息及OTA状态的读取、保存与清除
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 **/
 /* Includes ----------------------------------------------------------------------------------------------------------*/

#include "param.h"
#include "string.h"
#include "bsp_flash.h"


/* Flash Parameter Regions -------------------------------------------------------------------------------------------*/

/**
 * @brief Flash 参数存储分区
 * @note  每个分区占用一个 128 Byte Flash 页。
 *
 *        故障记录：0x0800FE00 ~ 0x0800FE7F，128 Byte
 *        系统参数：0x0800FE80 ~ 0x0800FEFF，128 Byte
 *        SOC 信息：0x0800FF00 ~ 0x0800FF7F，128 Byte
 *        OTA 状态：0x0800FF80 ~ 0x0800FFFF，128 Byte
 */
const param_list_t g_param_list[PARAM_ID_MAX] =
{
    [PARAM_ID_FAULT]  = {.addr = 0x0800FE00UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_SYSTEM] = {.addr = 0x0800FE80UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_SOC]    = {.addr = 0x0800FF00UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_OTA]    = {.addr = 0x0800FF80UL, .pageNum = 1U, .size = 0x80U}
};

/** Flash 页数据缓存，用于页读取、修改及写回操作 */
flash_page_t pageData = {0};

/*******************************************************************************
//  Function   : param_soc_read
//  Description: 从 Flash 读取电池 SOC 持久化数据
//  Param      : buffer - 接收 SOC 数据的 RAM 缓冲区
//               length - 需要读取的数据长度，单位 Byte
//  Return     : None
//  Others     : 读取 SOC 参数分区并检查数据有效标识；
//               有效时复制保存的数据，无效时清零接收缓冲区；
//               最大读取长度为 PARAM_PAGE_SIZE - PARAM_WORD_SIZE
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_soc_read(uint8_t *buffer, uint8_t length)
{
    if (buffer == NULL)
    {
        return;
    }

    /* 限制读取长度，避免超过 Flash 参数数据区域。 */
    if (length > (PARAM_PAGE_SIZE - PARAM_WORD_SIZE))
    {
        length = PARAM_PAGE_SIZE - PARAM_WORD_SIZE;
    }

    /* 读取 SOC 参数 Flash 页。 */
    bsp_flash_PageRead(g_param_list[PARAM_ID_SOC].addr, pageData.word);

    /* 检查参数有效性标识。 */
    if (pageData.word[0] == DATA_VALID_ID)
    {
        memcpy(buffer, &pageData.byte[PARAM_WORD_SIZE], length);
    }
    else
    {
        memset(buffer, 0, length);
    }
}


/*******************************************************************************
//  Function   : param_soc_save
//  Description: 将电池 SOC 持久化数据保存至 Flash
//  Param      : buffer - 待保存的 SOC 数据缓冲区
//               length - 需要保存的数据长度，单位 Byte
//  Return     : None
//  Others     : 在 Flash 页起始位置写入数据有效标识；
//               剩余区域保存 SOC 数据；执行页擦除及重新编程；
//               最大保存长度为 PARAM_PAGE_SIZE - PARAM_WORD_SIZE
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_soc_save(const uint8_t *buffer, uint8_t length)
{
    if (buffer == NULL)
    {
        return;
    }

    /* 限制保存长度，为数据有效性标识预留空间。 */
    if (length > (PARAM_PAGE_SIZE - PARAM_WORD_SIZE))
    {
        length = PARAM_PAGE_SIZE - PARAM_WORD_SIZE;
    }

    /* 初始化 Flash 页缓存。 */
    memset(pageData.byte, 0xFF, PARAM_PAGE_SIZE);

    /* 设置有效性标识并复制 SOC 数据。 */
    pageData.word[0] = DATA_VALID_ID;
    memcpy(&pageData.byte[PARAM_WORD_SIZE], buffer, length);

    /* 进入 Flash 擦写临界区。 */
    __disable_irq();

    bsp_flash_Unlock();

    /* 擦除 SOC 参数页并写入最新数据。 */
    bsp_flash_PageErase(g_param_list[PARAM_ID_SOC].addr);
    bsp_flash_PageProgramming(g_param_list[PARAM_ID_SOC].addr, pageData.word);

    bsp_flash_Lock();

    __enable_irq();
}



/*******************************************************************************
//  Function   : param_ota_save
//  Description: 将 OTA 升级状态保存至 Flash
//  Param      : status - 当前 OTA 升级状态
//  Return     : None
//  Others     : 擦除 OTA 参数分区，将升级状态保存至页首字节；
//               其余未使用字节填充为 0xFF
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_ota_save(otc_status_enum_t status)
{
    __disable_irq();

    bsp_flash_Unlock();
    bsp_flash_PageErase(g_param_list[PARAM_ID_OTA].addr);

    memset(pageData.byte, 0xFF, PARAM_PAGE_SIZE);
    pageData.byte[0] = (uint8_t)status;

    bsp_flash_PageProgramming(g_param_list[PARAM_ID_OTA].addr, pageData.word);
    bsp_flash_Lock();

    __enable_irq();
}


/*******************************************************************************
//  Function   : param_ota_state
//  Description: 读取 Flash 中保存的 OTA 升级状态
//  Param      : None
//  Return     : Flash 中保存的 OTA 状态枚举值
//  Others     : 直接读取 OTA 参数分区首字节，不执行 Flash 擦写；
//               当前未对未初始化值及非法状态编码进行校验
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
otc_status_enum_t param_ota_state(void)
{
    return (otc_status_enum_t)*(volatile const uint8_t *)
           (g_param_list[PARAM_ID_OTA].addr);
}

/*******************************************************************************
//  Function   : param_Init
//  Description: 初始化Flash参数管理模块
//  Param      : None
//  Return     : None
//  Others     : 根据功能使能配置读取产品信息、故障记录和按键记录
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void param_Init(void)
{

}

/*******************************************************************************
//  Function   : param_Restore
//  Description: 恢复运行参数
//  Param      : value - 恢复触发值，等于1时执行恢复操作
//  Return     : None
//  Others     : 清零按键记录和故障记录，并重新读取产品信息
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
void param_Restore(uint8_t value)
{

}
