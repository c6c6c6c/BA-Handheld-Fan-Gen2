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

/* Variables ---------------------------------------------------------------------------------------------------------*/
/*
 * Flash参数存储分区
 *
 * 按键记录：       0x0800FA80～0x0800FE7F，1024 Byte，8页
 * 故障记录：       0x0800FE80～0x0800FEFF，128 Byte，1页
 * 产品信息：       0x0800FF00～0x0800FF7F，128 Byte，1页
 * OTA状态：        0x0800FF80～0x0800FFFF，128 Byte，1页
 */
const param_list_t g_param_list[PARAM_ID_MAX] =
{
    #if PARAM_KEY_EN
    [PARAM_ID_KEY]     = {.addr = 0x0800FA80UL, .pageNum = BUTTON_RECORD_PAGE_NUM, .size = 0x400U},
    #endif
    #if PARAM_FAULT_EN
    [PARAM_ID_FAULT]   = {.addr = 0x0800FE80UL, .pageNum = 1U, .size = 0x80U },
    #endif
    #if PARAM_PRODUCT_EN
    [PARAM_ID_PRODUCT] = {.addr = 0x0800FF00UL, .pageNum = 1U, .size = 0x80U },
    #endif
	
    [PARAM_ID_OTA]     = {.addr = 0x0800FF80UL, .pageNum = 1U, .size = 0x80U }
};


flash_page_t pageData = {0};
#if PARAM_KEY_EN
    button_record_t param_button;  // 按键记录数据
#endif
#if PARAM_FAULT_EN
    fault_record_t param_fault;    // 故障记录数据
#endif
#if PARAM_PRODUCT_EN
    product_info_t param_product;  // 产品信息数据
#endif
/*******************************************************************************
//  Function   : param_fault_clear
//  Description: 清除故障记录，并擦除故障数据所在的Flash页
//  Param      : None
//  Return     : None
//  Others     : 同时清零RAM中的故障记录缓存
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_FAULT_EN
void param_fault_clear(void)
{
    memset((void *)&param_fault, 0, sizeof(fault_record_t));
    __disable_irq();
    bsp_flash_Unlock();
    bsp_flash_PageErase(g_param_list[PARAM_ID_FAULT].addr);
    bsp_flash_Lock();
    __enable_irq();
}
#endif
/*******************************************************************************
//  Function   : param_fault_read
//  Description: 从Flash读取故障记录到RAM缓存
//  Param      : None
//  Return     : None
//  Others     : 仅当数据有效标识正确时更新故障记录
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_FAULT_EN
void param_fault_read(void)
{
    bsp_flash_PageRead(g_param_list[PARAM_ID_FAULT].addr, pageData.word);
    
    if (pageData.word[0] == DATA_VALID_ID)
    {
        param_fault.cnt = pageData.word[1];
        param_fault.idx = pageData.word[2];
        for (uint8_t i = 0; i < FAULT_RECORD_NUMBER; i++)
        {
            param_fault.buf[i] = pageData.word[i + 3];
        }
    }
}
#endif
/*******************************************************************************
//  Function   : param_fault_save
//  Description: 保存故障记录到Flash
//  Param      : count  - 故障计数值
//               buffer - 待保存的故障数据缓冲区；为NULL时记录通信故障码
//               length - buffer中的有效数据长度
//  Return     : None
//  Others     : 写入前擦除故障数据页，写入完成后重新读取并更新RAM缓存
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_FAULT_EN
void param_fault_save(uint32_t count, uint8_t *buffer, uint8_t length)
{
    uint8_t i;

    if (length > PARAM_PAGE_SIZE - 8)
    {
        length = PARAM_PAGE_SIZE - 8;
    }

    bsp_flash_PageRead(g_param_list[PARAM_ID_FAULT].addr, pageData.word);
    if (pageData.word[0] != DATA_VALID_ID)
    {
        memset(pageData.word, 0, PARAM_PAGE_SIZE);
    }
    if ((buffer != NULL) && (length > 0))
    {
        pageData.word[0] = DATA_VALID_ID;
        pageData.word[1] = count;
        pageData.word[2] = param_fault.idx;
        memcpy(&pageData.byte[12], buffer, length);
    }
    else
    {
        if (param_fault.idx >= FAULT_RECORD_NUMBER)
        {
            for (i = 0; i < FAULT_RECORD_NUMBER - 1; i++)
            {
                param_fault.buf[i] = param_fault.buf[i + 1];
            }
            param_fault.buf[FAULT_RECORD_NUMBER - 1] = COMMUNICATION_FAULT_CODE;
        }
        else
        {
            param_fault.buf[param_fault.idx] = COMMUNICATION_FAULT_CODE;
            param_fault.idx++;
        }
        pageData.word[0] = DATA_VALID_ID;
        pageData.word[1] = count;
        pageData.word[2] = param_fault.idx;
        for (i = 0; i < FAULT_RECORD_NUMBER; i++)
        {
            pageData.word[i + 3] = param_fault.buf[i];
        }
    }
    __disable_irq();
    bsp_flash_Unlock();
    bsp_flash_PageErase(g_param_list[PARAM_ID_FAULT].addr);
    bsp_flash_PageProgramming(g_param_list[PARAM_ID_FAULT].addr, pageData.word);
    bsp_flash_Lock();
    __enable_irq();

    bsp_flash_PageRead(g_param_list[PARAM_ID_FAULT].addr, pageData.word);
    for (i = 0; i < FAULT_RECORD_NUMBER; i++)
    {
        param_fault.buf[i] = pageData.word[i + 3];
    }
}
#endif
/*******************************************************************************
//  Function   : param_product_read
//  Description: 从Flash读取产品信息到RAM缓存
//  Param      : None
//  Return     : None
//  Others     : 数据无效时使用字符'-'填充产品信息缓存
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_PRODUCT_EN
void param_product_read(void)
{
    bsp_flash_PageRead(g_param_list[PARAM_ID_PRODUCT].addr, pageData.word);
    if (pageData.word[0] == DATA_VALID_ID)
    {
        memcpy(param_product.buf, &pageData.byte[4], INFO_SIZE);
    }
    else
    {
        memset(param_product.buf, '-', INFO_SIZE);
    }
}
#endif
/*******************************************************************************
//  Function   : param_product_save
//  Description: 保存产品信息到Flash
//  Param      : buffer - 待保存的产品信息缓冲区
//               length - 待保存的数据长度
//  Return     : None
//  Others     : 保存完成后重新读取Flash并更新产品信息RAM缓存
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_PRODUCT_EN
void param_product_save(uint8_t *buffer, uint8_t length)
{
    if (length > PARAM_PAGE_SIZE)
    {
        length = PARAM_PAGE_SIZE;
    }

    bsp_flash_PageRead(g_param_list[PARAM_ID_PRODUCT].addr, pageData.word);
    pageData.word[0] = DATA_VALID_ID;
    memcpy(&pageData.byte[4], buffer, length);
    __disable_irq();
    bsp_flash_Unlock();
    bsp_flash_PageErase(g_param_list[PARAM_ID_PRODUCT].addr);
    bsp_flash_PageProgramming(g_param_list[PARAM_ID_PRODUCT].addr, pageData.word);
    bsp_flash_Lock();
    __enable_irq();

    bsp_flash_PageRead(g_param_list[PARAM_ID_PRODUCT].addr, pageData.word);
    memcpy(param_product.buf, &pageData.byte[4], INFO_SIZE);
}
#endif

/*******************************************************************************
//  Function   : param_ota_save
//  Description: 保存OTA升级状态到Flash
//  Param      : status - OTA升级状态
//  Return     : None
//  Others     : 擦除OTA状态页后，将状态值写入该页首字节
//  Author     : 陈亮 2026-06-06
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
//  Description: 保存OTA升级状态到Flash
//  Param      : status - OTA升级状态
//  Return     : None
//  Others     : 擦除OTA状态页后，将状态值写入该页首字节
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
otc_status_enum_t param_ota_state(void)
{
	return (otc_status_enum_t)*(uint8_t *)(g_param_list[PARAM_ID_OTA].addr);
}


/*******************************************************************************
//  Function   : param_button_clear
//  Description: 清除按键记录数据
//  Param      : None
//  Return     : None
//  Others     : 擦除按键记录区域使用的全部Flash页
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_KEY_EN
void param_button_clear(void)
{
    uint8_t i;
    __disable_irq();
    bsp_flash_Unlock();
    for (i = 0; i < 8; i++)
    {
        bsp_flash_PageErase(g_param_list[PARAM_ID_KEY].addr + (i * PARAM_PAGE_SIZE));
    }
    bsp_flash_Lock();
    __enable_irq();
}
#endif

/*******************************************************************************
//  Function   : param_button_read
//  Description: 从Flash读取最新一组按键记录
//  Param      : None
//  Return     : None
//  Others     : 遍历所有记录项，通过累计保存次数确定最新记录位置
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_KEY_EN
void param_button_read(void)
{
    uint8_t i,j;

    if (bsp_flash_WordRead(g_param_list[PARAM_ID_KEY].addr) > BUTTON_VALUE_SAVE_TIMES_MAX)
    {
        memset(&param_button, 0, sizeof(param_button));
        return;
    }

	param_button.total = 0;
	param_button.idx   = 0;
    for (i = 0; i < BUTTON_RECORD_PAGE_NUM; i++)
    {
        for(j = 0 ; j < BUTTON_RECORD_ITEM_SIZE ; j++)
        {
            uint32_t Addr = g_param_list[PARAM_ID_KEY].addr 
                          + (i * PARAM_PAGE_SIZE)
                          + (j * BUTTON_RECORD_ITEM * sizeof(uint32_t));
            uint32_t Val = bsp_flash_WordRead(Addr);
            if(Val > param_button.total && Val < BUTTON_VALUE_SAVE_TIMES_MAX)
            {
                param_button.total = Val;
                param_button.idx   = i * BUTTON_RECORD_ITEM_SIZE + j;
            }
        }
    }

    for(i = 1 ; i <= BUTTON_RECORD_NUM ; i++)
    {
        param_button.data[i - 1] = bsp_flash_WordRead(g_param_list[PARAM_ID_KEY].addr 
			                           + BUTTON_GET_PAGE_OFFSET(param_button.idx,i));
		if(param_button.data[i - 1] == 0xFFFFFFFF)
		{
				param_button.data[i - 1] = 0;
		}
    }
}
#endif

/*******************************************************************************
//  Function   : param_button_save
//  Description: 保存当前按键记录到Flash
//  Param      : None
//  Return     : None
//  Others     : 记录项循环使用；累计保存次数达到上限时清除记录区并重新计数
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_KEY_EN
void param_button_save(void)
{
    uint32_t next_idx;
    uint32_t page_num;
    uint32_t word_offset;
    uint32_t address;
    uint8_t  i;

    if (param_button.total >= BUTTON_VALUE_SAVE_TIMES_MAX)
    {
        param_button_clear();
        memset(&param_button, 0, sizeof(param_button));
    }

    next_idx = param_button.total == 0U ? 0U : param_button.idx + 1U;

    if (next_idx >= BUTTON_RECORD_ITEM_TOTAL)
    {
        next_idx = 0U;
    }

    page_num = BUTTON_GET_PAGE_NUM(next_idx);
    word_offset = ((next_idx % BUTTON_RECORD_ITEM_SIZE) * BUTTON_RECORD_ITEM);
    address = g_param_list[PARAM_ID_KEY].addr + (page_num * PARAM_PAGE_SIZE);

    bsp_flash_PageRead(address, pageData.word);

    pageData.word[word_offset + 0U] = param_button.total + 1U;
    for(i = 1 ; i <= BUTTON_RECORD_NUM ; i++)
    {
        pageData.word[word_offset + i] = param_button.data[i - 1];
    }
    __disable_irq();
    bsp_flash_Unlock();
    bsp_flash_PageErase(address);
    bsp_flash_PageProgramming(address, pageData.word);
    bsp_flash_Lock();
    __enable_irq();

    param_button.total++;
    param_button.idx = next_idx;
}
#endif

/*******************************************************************************
//  Function   : param_button_getCnt
//  Description: 获取指定按键记录项的累计计数值
//  Param      : Item - 按键记录项索引
//  Return     : 指定记录项的计数值；索引无效时返回0
//  Others     : None
//  Author     : 陈亮 2026-06-06
*******************************************************************************/
#if PARAM_KEY_EN
uint32_t param_button_getCnt(uint8_t Item)
{
    if(Item < BUTTON_RECORD_NUM)
    {
        return param_button.data[Item];
    }
    return 0;
}
#endif

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
    #if PARAM_PRODUCT_EN
        param_product_read();
    #endif
    #if PARAM_FAULT_EN
        param_fault_read();
    #endif
    #if PARAM_KEY_EN
        param_button_read();
    #endif
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
    uint8_t i;
    if (value != 1)
    {
        return;
    }

    for(i = 0 ; i < BUTTON_RECORD_NUM ; i++)
    {
        param_button.data[i] = 0;
    }
    param_button_clear();


    param_fault.cnt = 0;
    param_fault.idx = 0;
    param_fault_clear();

    param_product_read();
}
