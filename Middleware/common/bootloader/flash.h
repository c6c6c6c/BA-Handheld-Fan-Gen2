
/**
 ***********************************************************************************************************************
 * @file    flash.h
 * @brief   Flash 存储器底层操作，实现 Flash 编程、Option Bytes 写入及擦除功能
 * @author  Puya Application Team
 ***********************************************************************************************************************
 * @attention
 *
 * Copyright (c) 2021 Puya Semiconductor.
 * All rights reserved.
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef __FLASH_H
#define __FLASH_H


/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "py32f0xx.h"


/* Flash Return Status -----------------------------------------------------------------------------------------------*/

#define RET_ERROR_NACK                  ERROR   /** Flash 操作失败，返回错误应答 NACK */
#define RET_SUCCESS_ACK                 SUCCESS /** Flash 操作成功，返回成功应答 ACK */

/* System Memory Address ---------------------------------------------------------------------------------------------*/

/** 系统存储区起始地址 */
#define SYSTEM_BASE                     (0x1FFF0000UL)

/** BID 信息存储地址，相对于系统存储区起始地址偏移 0xBFC */
#define BID_BASE                        (SYSTEM_BASE + 0x00000BFCUL)


/* Option Bytes Address ----------------------------------------------------------------------------------------------*/

#define OPTION_BASE                     (0x1FFF0F80UL)               /** Option Bytes 配置存储区起始地址 */
#define OPTR1_BASE                      (OPTION_BASE + 0x00000000UL) /** Option Bytes 配置字 1 存储地址 */
#define OPTR2_BASE                      (OPTION_BASE + 0x00000004UL) /** Option Bytes 配置字 2 存储地址 */
#define WRPR_BASE                       (OPTION_BASE + 0x00000010UL) /** Flash 写保护配置字存储地址 */
#define PCROP0SR_BASE                   (OPTION_BASE + 0x00000014UL) /** PCROP0 专有代码读保护起始地址配置字 */
#define PCROP0ER_BASE                   (OPTION_BASE + 0x00000018UL) /** PCROP0 专有代码读保护结束地址配置字 */


/* Device Identification ---------------------------------------------------------------------------------------------*/

/**
 * @brief MCU 设备标识读取宏
 * @note  读取 DBGMCU->IDCODE 寄存器；
 *        通过 IDCODE_DEVID_MASK 提取低 12 位设备标识。
 */
#define DBGMCU_IDCODE                   ((DBGMCU->IDCODE) & IDCODE_DEVID_MASK)

/** DBGMCU IDCODE 寄存器设备标识位掩码 */
#define IDCODE_DEVID_MASK               (0x00000FFFUL)


/* Option Bytes Configuration ----------------------------------------------------------------------------------------*/

/**
 * @brief Option Bytes 配置字 1 预设值
 * @note  保留原厂配置值；
 *        对应配置涉及读保护及其他系统选项；
 *        具体位域定义应参考目标芯片参考手册。
 */
#define FLASH_OPTR1                     0x1F55E0AA

/**
 * @brief Option Bytes 配置字 2 预设值
 * @note  对应配置的具体含义应参考目标芯片参考手册。
 */
#define FLASH_OPTR2                     0xFFFD0002

/**
 * @brief Flash 写保护配置字预设值
 * @note  用于配置各 Flash 扇区的写保护状态。
 */
#define FLASH_WRPR                      0x0000FFFF

#define FLASH_PCROP0SR                  0xFE0001FF /** PCROP0 专有代码读保护起始地址配置字预设值 */
#define FLASH_PCROP0ER                  0xFFFF0000 /** PCROP0 专有代码读保护结束地址配置字预设值 */


/* Flash Size Information --------------------------------------------------------------------------------------------*/

/**
 * @brief Flash 容量信息存储地址
 * @note  保留原厂代码定义；
 *        使用前应根据目标芯片型号及存储器映射确认地址有效性。
 */
#define FLASHSIZE_BASE                  (0x1FFFF1160UL)


/* STM32 Compatibility Address ---------------------------------------------------------------------------------------*/


#define STM32F0_FLASHSIZE_BASE          (0x1FFFF7CCUL) /** STM32F0 系列 Flash 容量信息存储地址 */
#define STM32F1_FLASHSIZE_BASE          (0x1FFFF7E0UL) /** STM32F1 系列 Flash 容量信息存储地址 */
#define ST_UID_BASE                     (0x1FFFF7E8UL) /** STM32 兼容 UID 信息存储地址 */
#define ST_OPTION_BASE                  (0x1FFFF800UL) /** STM32 兼容 Option Bytes 存储区起始地址 */
#define ST_BID_BASE                     (0x1FFFF7A6UL) /** STM32 兼容 BID 信息存储地址 */


/* Flash Timing Configuration ----------------------------------------------------------------------------------------*/

/**
 * @brief 配置 Flash 擦除及编程时序寄存器
 *
 * @param __EPPARA0__ Flash 时序参数组 0
 *                    Bit[7:0]   -> FLASH_TS0
 *                    Bit[15:8]  -> FLASH_TS3
 *                    Bit[24:16] -> FLASH_TS1
 *
 * @param __EPPARA1__ Flash 时序参数组 1
 *                    Bit[7:0]   -> FLASH_TS2P
 *                    Bit[26:16] -> FLASH_TPS3
 *
 * @param __EPPARA2__ Flash 时序参数组 2
 *                    Bit[16:0]  -> FLASH_PERTPE
 *
 * @param __EPPARA3__ Flash 时序参数组 3
 *                    Bit[16:0]  -> FLASH_SMERTPE
 *
 * @param __EPPARA4__ Flash 时序参数组 4
 *                    Bit[15:0]  -> FLASH_PRGTPE
 *                    Bit[29:16] -> FLASH_PRETPE
 *
 * @note  根据输入参数提取各 Flash 时序配置位域，
 *        并写入对应的 Flash 控制器时序寄存器。
 *
 *        时序参数应与目标芯片及当前 Flash 工作时钟匹配，
 *        不正确的时序配置可能导致 Flash 编程或擦除异常。
 *
 *        本宏仅执行 Flash 时序寄存器配置，
 *        不执行 Flash 解锁、擦除、编程及操作状态检查。
 *
 *        部分宏参数会被重复使用，
 *        调用时不应传入具有自增、自减等副作用的表达式。
 */
#define __HAL_FLASH_TIME_REG_SET(__EPPARA0__, __EPPARA1__, __EPPARA2__, __EPPARA3__, __EPPARA4__)    \
    do                                                                                               \
    {                                                                                                \
        FLASH->TS0     = (__EPPARA0__) & 0xFF;                                                       \
        FLASH->TS1     = ((__EPPARA0__) >> 16) & 0x1FF;                                              \
        FLASH->TS3     = ((__EPPARA0__) >> 8) & 0xFF;                                                \
        FLASH->TS2P    = (__EPPARA1__) & 0xFF;                                                       \
        FLASH->TPS3    = ((__EPPARA1__) >> 16) & 0x7FF;                                              \
        FLASH->PERTPE  = (__EPPARA2__) & 0x1FFFF;                                                    \
        FLASH->SMERTPE = (__EPPARA3__) & 0x1FFFF;                                                    \
        FLASH->PRGTPE  = (__EPPARA4__) & 0xFFFF;                                                     \
        FLASH->PRETPE  = ((__EPPARA4__) >> 16) & 0x3FFF;                                             \
    } while (0U)


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : WriteFlash
//  Description: 将指定数据编程至 Flash 存储器
//  Param      : dwAddr       - Flash 目标写入地址
//               pucDataBuf   - 待写入数据缓冲区首地址
//               ucDataLength - 数据长度编码，实际长度为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - Flash 编程成功
//               RET_ERROR_NACK  - 参数错误、写保护或 Flash 编程失败
//  Others     : Flash 每页为 128 Byte，包含 32 个 32 位数据字；
//               未启用 ADJUST_ADDR_AND_SIZE 时，
//               写入地址及实际长度必须按照 128 Byte 对齐；
//               启用自动对齐功能时，可能修改并扩展输入缓冲区；
//               CHECK_WRP 使能时检查对应扇区写保护状态；
//               调用前应完成 Flash 解锁及空闲状态检查；
//               本函数不负责 Flash 擦除及数据回读校验
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus WriteFlash(uint32_t dwAddr,
                       uint8_t *pucDataBuf,
                       uint8_t ucDataLength);


/*******************************************************************************
//  Function   : WriteOption
//  Description: 写入 Flash Option Bytes 配置
//  Param      : dwAddr       - Option Bytes 目标写入地址
//               pucDataBuf   - 待写入配置数据缓冲区首地址
//               ucDataLength - 数据长度编码，实际长度为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - Option Bytes 写入成功
//               RET_ERROR_NACK  - Option Bytes 写入失败
//  Others     : 读取当前 OPTR、WRPR 及 PCROPR 寄存器；
//               根据目标地址更新对应配置字节；
//               保留未指定修改的配置字段；
//               自动执行 Option Bytes 专用解锁；
//               写入成功后设置 OBL_LAUNCH，重新加载 Option Bytes；
//               重新加载配置可能触发系统复位
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus WriteOption(uint32_t dwAddr,
                        uint8_t *pucDataBuf,
                        uint8_t ucDataLength);


/*******************************************************************************
//  Function   : MassErase
//  Description: 执行 Flash 整片擦除操作
//  Param      : None
//  Return     : RET_SUCCESS_ACK - Flash 整片擦除成功
//               RET_ERROR_NACK  - Flash 整片擦除失败
//  Others     : 设置 FLASH_CR.MER，进入整片擦除模式；
//               向 Flash 地址写入 32 位数据触发擦除；
//               等待 BSY 清零，并通过 EOP 标志判断执行结果；
//               调用前应完成 Flash 解锁及空闲状态检查；
//               操作可能擦除整个用户 Flash 区域，
//               使用前应确认 Bootloader、Application 及参数区保护要求
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus MassErase(void);


/*******************************************************************************
//  Function   : PageErase
//  Description: 根据指定页组信息执行 Flash 页擦除
//  Param      : dwAddr    - 待擦除页地址
//  Return     : RET_SUCCESS_ACK - 指定 Flash 页擦除成功
//               RET_ERROR_NACK  - Flash 写保护或擦除失败
//  Others     : 原厂实现使用 128 Byte 作为 Flash 页大小；
//               根据页组编号及组内页索引计算目标页地址；
//               CHECK_WRP 使能时检查对应扇区写保护；
//               擦除过程中发生错误时终止后续擦除；
//               Flash 解锁及初始空闲状态检查由外部流程负责
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus PageErase(uint32_t dwAddr);


/*******************************************************************************
//  Function   : SectorErase
//  Description: 根据指定扇区编号执行 Flash 扇区擦除
//  Param      : pwDataBuf    - 待擦除 Flash 扇区编号缓冲区
//               ucDataLength - 扇区数量编码，实际数量为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - Flash 扇区擦除成功
//               RET_ERROR_NACK  - Flash 写保护或扇区擦除失败
//  Others     : 原厂实现使用 0x1000 Byte，即 4KB 作为扇区大小；
//               目标地址 = FLASH_BASE + 扇区编号 * 0x1000；
//               CHECK_WRP 使能时检查对应扇区写保护状态；
//               发生擦除错误时终止后续操作；
//               调用方应保证扇区编号及擦除范围有效；
//               调用前应完成 Flash 解锁及空闲状态检查
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus SectorErase(uint16_t *pwDataBuf,
                        uint8_t ucDataLength);


#endif /* __FLASH_H */


/************************ (C) COPYRIGHT Puya Semiconductor *****END OF FILE****/
