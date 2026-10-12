/**
 ***********************************************************************************************************************
 * @file    bsp_flash.h
 * @brief   Flash 页操作、数据校验及芯片 UID 读取接口
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _BSP_FLASH_H_
#define _BSP_FLASH_H_

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/
/* BOR 等级配置常量；本头文件中的定义本身不会修改选项字节。 */
#define BOR_LEVEL                       LL_FLASH_BOR_LEV0
/* PY32F032 芯片 UID 区域起始地址。 */
#define FLASH_UID_BASE                  0x1FFF0F00UL
/* Typedef -----------------------------------------------------------------------------------------------------------*/

/* Function declarations ---------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : bsp_flash_Lock
//  Description: 锁定 Flash 擦除及编程操作
//  Param      : None
//  Return     : None
//  Others     : 写操作结束后调用；锁定不影响存储器读取
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_Lock(void);

/*******************************************************************************
//  Function   : bsp_flash_Unlock
//  Description: 解锁 Flash 擦除及编程操作
//  Param      : None
//  Return     : None
//  Others     : 擦除或编程前调用，操作结束后应调用 bsp_flash_Lock()
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_Unlock(void);

/*******************************************************************************
//  Function   : bsp_flash_PageErase
//  Description: 擦除指定 Flash 页面
//  Param      : addr - 待擦除页面地址，应按 FLASH_PAGE_SIZE 对齐
//  Return     : None
//  Others     : 调用前需解锁 Flash；阻塞等待 BUSY/EOP，无超时及错误返回
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_PageErase(uint32_t addr);

/*******************************************************************************
//  Function   : bsp_flash_ReadUID
//  Description: 读取芯片 UID 区域数据
//  Param      : buffer - 接收缓冲区，至少容纳 4 个 uint32_t（16 字节）
//  Return     : None
//  Others     : 当前实现读取连续 4 个字；标准 96 位 UID 为前 3 个字，
//               第 4 个字不属于标准 UID，调用方不应将其纳入 UID
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_ReadUID(uint32_t *buffer);

/*******************************************************************************
//  Function   : bsp_flash_WordRead
//  Description: 读取 Flash 中的一个 32 位字
//  Param      : addr - 待读取地址，应按 4 字节对齐
//  Return     : 指定地址处的 32 位数据
//  Others     : 直接读取存储器，调用方负责地址有效性
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
uint32_t bsp_flash_WordRead(uint32_t addr);

/*******************************************************************************
//  Function   : bsp_flash_PageRead
//  Description: 读取一个 Flash 页面的数据
//  Param      : addr - 待读取页面起始地址，应按 4 字节对齐
//               buffer - 接收缓冲区，至少容纳 FLASH_PAGE_SIZE / 4 个 uint32_t
//  Return     : None
//  Others     : 按 32 位读取 FLASH_PAGE_SIZE 字节；调用方负责地址及缓冲区有效性
//  Author     : Chen Liang 2026-10-12
*******************************************************************************/
void bsp_flash_PageRead(uint32_t addr, uint32_t *buffer);

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
bool bsp_flash_PageVerify(uint32_t addr, uint32_t *buffer, uint32_t length);

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
void bsp_flash_PageProgramming(uint32_t addr, uint32_t *buffer);

#endif /* _BSP_FLASH_H_ */
