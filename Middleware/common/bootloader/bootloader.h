
/**
 ***********************************************************************************************************************
 * @file    bootloader.h
 * @brief   Bootloader 固件升级接口，定义 Application Flash 分区及 OTA 烧录控制参数
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-11-02
 ***********************************************************************************************************************
 * @note
 *          1. 定义 MCU Flash、Bootloader 及 Application 存储地址。
 *          2. 定义 OTA 升级过程中的固件烧录地址及进度信息。
 *          3. 提供 Flash 页数据缓存，支持字节和 32 位字访问。
 *          4. 提供 OTA 初始化及固件烧录接口。
 *          5. Application 烧录区域不得覆盖 Flash 参数存储分区。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef BOOTLOADER_H
#define BOOTLOADER_H

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** MCU 内部 Flash 起始地址 */
#define MCU_BASE_ADDRESS            0x08000000UL

/** Application 相对于 MCU Flash 起始地址的偏移量 */
#define APP_FLASH_OFFSET            0x4000UL

/** Application 固件存储起始地址 */
#define APP_BASE_ADDRESS            (MCU_BASE_ADDRESS + APP_FLASH_OFFSET)

/**
 * @brief Application 固件存储结束地址
 * @note  采用左闭右开地址区间 [APP_BASE_ADDRESS, APP_END_ADDRESS)。
 *        APP_END_ADDRESS 为参数区起始地址，不属于 Application 区域。
 */
#define APP_END_ADDRESS             0x0800FE00UL

/** Application 固件最大允许长度，单位 Byte */
#define APP_MAX_SIZE                (APP_END_ADDRESS - APP_BASE_ADDRESS)

/** Flash 每页包含的 32 位数据字数量 */
#define BOOT_FLASH_PAGE_WORDS       (FLASH_PAGE_SIZE / sizeof(uint32_t))


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief Bootloader 固件升级控制结构体
 * @note  保存 OTA 固件烧录进度、当前数据包长度及 Flash 页缓存。
 *        每次成功烧录一页后，更新 addr、size、count 和 pack_size。
 */
typedef struct
{
    uint32_t addr;               /**< 当前 Flash 烧录地址 */
    uint32_t size;               /**< 已成功烧录的固件有效数据总长度，单位 Byte */

    uint16_t count;              /**< 已成功烧录的 Flash 数据包数量 */
    uint16_t pack_size;          /**< 最近一次烧录的数据包长度，单位 Byte */

    /**
     * @brief Flash 页数据缓存
     * @note  byte[] 和 word[] 共用同一块存储空间。
     *        byte[] 用于接收和填充固件数据；
     *        word[] 用于调用 Flash 页编程和校验接口。
     */
    union
    {
        uint8_t  byte[FLASH_PAGE_SIZE];         /**< Flash 页字节缓存 */
        uint32_t word[BOOT_FLASH_PAGE_WORDS];   /**< Flash 页 32 位字缓存 */
    } buffer;

} boot_update_t;

typedef void (*func_pointer_t)(void);

/* External variables ------------------------------------------------------------------------------------------------*/

/** Bootloader OTA 升级控制实例 */
extern boot_update_t boot_cmd;


/* Function declarations ---------------------------------------------------------------------------------------------*/
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
void JumpToApplication(void);

/*******************************************************************************
//  Function   : bootloader_Init
//  Description: 初始化 Bootloader OTA 固件升级参数
//  Param      : None
//  Return     : None
//  Others     : 将烧录地址设置为 APP_BASE_ADDRESS；
//               清零固件大小、数据包数量及最近一次包长度；
//               初始化 Flash 页缓存并保存 OTA 编程状态
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
void bootloader_Init(void);


/*******************************************************************************
//  Function   : bootloader_burn
//  Description: 将当前 OTA 固件数据包烧录至 Application Flash
//  Param      : buffer - 待烧录固件数据缓冲区
//               length - 当前固件数据包长度，单位 Byte
//  Return     : true  - 当前 Flash 页烧录及数据校验成功
//               false - 参数无效、地址越界或 Flash 校验失败
//  Others     : 每次调用处理一个 Flash 页；
//               数据不足一页时，剩余区域使用 0xFF 填充；
//               校验成功后自动更新烧录地址、固件长度及包计数
//  Author     : Chen Liang 2026-11-02
*******************************************************************************/
bool bootloader_burn(const uint8_t *buffer, uint8_t length);


#endif /* BOOTLOADER_H */
