
/**
 ***********************************************************************************************************************
 * @file    param.c
 * @brief   Flash 参数管理，实现系统参数、SOC 信息及 OTA 状态的读取、保存与恢复
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "param.h"
#include "string.h"
#include "bsp_flash.h"


/* Flash Parameter Regions -------------------------------------------------------------------------------------------*/

/**
 * @brief Flash 参数存储分区配置表
 * @note  每个参数分区占用一个独立的 128 Byte Flash 页。
 *
 *        参数分区       起始地址      结束地址      大小
 *        FAULT          0x0800FE00    0x0800FE7F    128 Byte
 *        SYSTEM         0x0800FE80    0x0800FEFF    128 Byte
 *        SOC            0x0800FF00    0x0800FF7F    128 Byte
 *        OTA            0x0800FF80    0x0800FFFF    128 Byte
 *
 *        addr    ：参数分区起始地址。
 *        pageNum ：参数分区占用的 Flash 页数量。
 *        size    ：参数分区总大小，单位 Byte。
 *
 *        普通参数页采用有效标识 + 参数数据的存储结构。
 *        OTA 状态页直接使用首字节保存升级状态。
 */
const param_list_t g_param_list[PARAM_ID_MAX] =
{
    [PARAM_ID_FAULT]  = {.addr = 0x0800FE00UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_SYSTEM] = {.addr = 0x0800FE80UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_SOC]    = {.addr = 0x0800FF00UL, .pageNum = 1U, .size = 0x80U},
    [PARAM_ID_OTA]    = {.addr = 0x0800FF80UL, .pageNum = 1U, .size = 0x80U}
};


/* Variables ---------------------------------------------------------------------------------------------------------*/

/**
 * @brief Flash 参数页读写缓存
 * @note  用于保存完整 Flash 页数据。
 *        支持按字节访问及按 32 位数据字访问；
 *        由参数读取、保存及 OTA 状态写入接口共用。
 */
flash_page_t pageData = {0};


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : param_read
//  Description: 从指定 Flash 参数分区读取持久化数据
//  Param      : buffer - 参数数据接收缓冲区
//               length - 请求读取的数据长度，单位 Byte
//               idx    - 参数分区索引，对应 PARAM_ID_*
//  Return     : None
//  Others     : 检查接收缓冲区是否有效；
//               限制读取长度，避免超过单页有效数据区域；
//               根据分区索引读取对应 Flash 参数页；
//               检查页首 DATA_VALID_ID 有效性标识；
//               数据有效时复制至接收缓冲区；
//               数据无效时将接收缓冲区清零；
//               最大有效数据长度为 PARAM_PAGE_SIZE - PARAM_WORD_SIZE；
//               当前函数未检查 idx 是否超出 PARAM_ID_MAX
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_read(uint8_t *buffer, uint8_t length, uint8_t idx)
{
    /* 接收缓冲区无效时直接返回。 */
    if (buffer == NULL)
    {
        return;
    }

    /*
     * 限制参数读取长度。
     *
     * Flash 页首预留一个 32 位有效性标识，
     * 剩余空间用于存储实际参数数据。
     */
    if (length > (PARAM_PAGE_SIZE - PARAM_WORD_SIZE))
    {
        length = PARAM_PAGE_SIZE - PARAM_WORD_SIZE;
    }

    /* 根据参数分区索引读取对应 Flash 页。 */
    bsp_flash_PageRead(g_param_list[idx].addr, pageData.word);

    /*
     * 检查 Flash 参数页有效性。
     *
     * 有效：跳过页首标识，将参数数据复制至 RAM。
     * 无效：清零接收缓冲区，表示当前没有有效参数。
     */
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
//  Function   : param_save
//  Description: 将参数数据保存至指定 Flash 参数分区
//  Param      : buffer - 待保存的参数数据缓冲区
//               length - 待保存的数据长度，单位 Byte
//               idx    - 参数分区索引，对应 PARAM_ID_*
//  Return     : None
//  Others     : 检查输入缓冲区是否有效；
//               限制参数长度，避免超过单页有效数据区域；
//               将完整页缓存初始化为 0xFF；
//               在页首写入 DATA_VALID_ID 有效性标识；
//               将参数数据复制至有效标识之后；
//               关闭中断并解锁 Flash；
//               擦除对应参数页并执行整页编程；
//               操作结束后锁定 Flash 并重新使能中断；
//               当前函数未执行 idx 范围检查及写入结果校验
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_save(const uint8_t *buffer, uint8_t length, uint8_t idx)
{
    /* 待保存数据缓冲区无效时直接返回。 */
    if (buffer == NULL)
    {
        return;
    }

    /* 限制参数长度，为页首有效性标识预留空间。 */
    if (length > (PARAM_PAGE_SIZE - PARAM_WORD_SIZE))
    {
        length = PARAM_PAGE_SIZE - PARAM_WORD_SIZE;
    }

    /*
     * 初始化完整 Flash 页缓存。
     *
     * 将未使用区域填充为 0xFF，
     * 与 Flash 擦除后的默认状态保持一致。
     */
    memset(pageData.byte, 0xFF, PARAM_PAGE_SIZE);

    /*
     * 组织参数页数据：
     *
     * Byte[0..3] ：DATA_VALID_ID。
     * Byte[4..N] ：实际参数数据。
     * 剩余区域   ：0xFF。
     */
    pageData.word[0] = DATA_VALID_ID;
    memcpy(&pageData.byte[PARAM_WORD_SIZE], buffer, length);

    /*
     * 进入 Flash 擦写临界区。
     *
     * 避免普通中断在 Flash 擦写过程中打断操作。
     */
    __disable_irq();

    /* 解锁 Flash 控制器。 */
    bsp_flash_Unlock();

    /* 擦除指定参数分区对应的 Flash 页。 */
    bsp_flash_PageErase(g_param_list[idx].addr);

    /* 将完整参数页缓存写入 Flash。 */
    bsp_flash_PageProgramming(g_param_list[idx].addr, pageData.word);

    /* 重新锁定 Flash 控制器。 */
    bsp_flash_Lock();

    /* 重新使能全局中断。 */
    __enable_irq();
}


/*******************************************************************************
//  Function   : param_soc_read
//  Description: 从 Flash 读取电池 SOC 持久化参数
//  Param      : buffer - SOC 参数接收缓冲区
//               length - 请求读取的数据长度，单位 Byte
//  Return     : None
//  Others     : 调用通用参数读取接口 param_read()；
//               固定使用 PARAM_ID_SOC 参数分区；
//               由通用接口完成页读取及有效性检查
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_soc_read(uint8_t *buffer, uint8_t length)
{
    /* 从 SOC 专用参数分区读取持久化数据。 */
    param_read(buffer, length, PARAM_ID_SOC);
}


/*******************************************************************************
//  Function   : param_soc_save
//  Description: 将电池 SOC 持久化参数保存至 Flash
//  Param      : buffer - 待保存的 SOC 参数缓冲区
//               length - 待保存的数据长度，单位 Byte
//  Return     : None
//  Others     : 调用通用参数保存接口 param_save()；
//               固定使用 PARAM_ID_SOC 参数分区；
//               用于保存 SOC 及相关电池历史统计信息
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_soc_save(const uint8_t *buffer, uint8_t length)
{
    /* 将 SOC 参数写入独立的 Flash 存储分区。 */
    param_save(buffer, length, PARAM_ID_SOC);
}


/*******************************************************************************
//  Function   : param_system_read
//  Description: 从 Flash 读取系统运行配置参数
//  Param      : buffer - 系统参数接收缓冲区
//               length - 请求读取的数据长度，单位 Byte
//  Return     : None
//  Others     : 调用通用参数读取接口 param_read()；
//               固定使用 PARAM_ID_SYSTEM 参数分区；
//               由通用接口完成参数有效性检查及数据读取
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_system_read(uint8_t *buffer, uint8_t length)
{
    /* 从系统参数分区读取持久化配置。 */
    param_read(buffer, length, PARAM_ID_SYSTEM);
}


/*******************************************************************************
//  Function   : param_system_save
//  Description: 将系统运行配置参数保存至 Flash
//  Param      : buffer - 待保存的系统参数缓冲区
//               length - 待保存的数据长度，单位 Byte
//  Return     : None
//  Others     : 调用通用参数保存接口 param_save()；
//               固定使用 PARAM_ID_SYSTEM 参数分区；
//               保存前由通用接口组织有效标识及参数数据；
//               擦除并重新编程对应 Flash 参数页
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_system_save(const uint8_t *buffer, uint8_t length)
{
    /* 将系统参数保存至独立的 Flash 存储分区。 */
    param_save(buffer, length, PARAM_ID_SYSTEM);
}


/*******************************************************************************
//  Function   : param_ota_save
//  Description: 将 OTA 升级状态保存至 Flash
//  Param      : status - 待保存的 OTA 状态枚举值
//  Return     : None
//  Others     : 固定使用 PARAM_ID_OTA 参数分区；
//               关闭中断并解锁 Flash；
//               擦除 OTA 状态参数页；
//               初始化完整页缓存为 0xFF；
//               将 OTA 状态保存至页首第一个字节；
//               执行 Flash 页编程并重新锁定 Flash；
//               最后恢复全局中断；
//               OTA 状态采用独立格式，不使用 DATA_VALID_ID 标识
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
void param_ota_save(otc_status_enum_t status)
{
    /* 进入 Flash 擦写临界区。 */
    __disable_irq();

    /* 解锁 Flash 并擦除 OTA 状态参数页。 */
    bsp_flash_Unlock();
    bsp_flash_PageErase(g_param_list[PARAM_ID_OTA].addr);

    /*
     * 初始化 OTA 状态页缓存。
     *
     * Byte[0]   ：OTA 状态枚举值。
     * Byte[1..] ：0xFF。
     */
    memset(pageData.byte, 0xFF, PARAM_PAGE_SIZE);
    pageData.byte[0] = (uint8_t)status;

    /* 将 OTA 状态写入 Flash 参数页。 */
    bsp_flash_PageProgramming(g_param_list[PARAM_ID_OTA].addr, pageData.word);

    /* 锁定 Flash 控制器。 */
    bsp_flash_Lock();

    /* 重新使能全局中断。 */
    __enable_irq();
}


/*******************************************************************************
//  Function   : param_ota_state
//  Description: 读取 Flash 中保存的 OTA 升级状态
//  Param      : None
//  Return     : Flash 中保存的 OTA 状态枚举值
//  Others     : 直接读取 OTA 参数分区首字节；
//               将读取结果转换为 otc_status_enum_t；
//               不执行 Flash 解锁、擦除及编程操作；
//               当前未检查未初始化状态及非法状态编码
//  Author     : Chen Liang 2026-10-27
*******************************************************************************/
otc_status_enum_t param_ota_state(void)
{
    /* 直接读取 OTA 状态分区首字节。 */
    return (otc_status_enum_t)*(volatile const uint8_t *)
           (g_param_list[PARAM_ID_OTA].addr);
}


/*******************************************************************************
//  Function   : param_Init
//  Description: 初始化 Flash 参数管理模块
//  Param      : None
//  Return     : None
//  Others     : 预留参数管理模块初始化接口；
//               当前没有执行 Flash 读取、校验及状态恢复操作
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void param_Init(void)
{

}


/*******************************************************************************
//  Function   : param_Restore
//  Description: 执行 Flash 参数恢复操作
//  Param      : value - 参数恢复控制值
//  Return     : None
//  Others     : 预留系统参数恢复接口；
//               当前函数尚未实现任何参数清除或恢复操作；
//               value 参数当前未参与实际逻辑
//  Author     : Chen Liang 2026-06-06
*******************************************************************************/
void param_Restore(uint8_t value)
{

}
