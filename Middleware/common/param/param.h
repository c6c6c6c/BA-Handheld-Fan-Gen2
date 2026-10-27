
/**
 ***********************************************************************************************************************
 * @file    param.h
 * @brief   Flash 参数管理模块接口，定义参数分区、数据存储及 OTA 状态管理
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-02
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _PARAM_H_
#define _PARAM_H_

#include "main.h"


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** Flash 存储空间起始地址 */
#define FLASH_BASE_ADDR             FLASH_BASE

/** Flash 存储空间结束地址 */
#define FLASH_END_ADDR              FLASH_END

/** Flash 单页大小，单位 Byte */
#define PARAM_PAGE_SIZE             FLASH_PAGE_SIZE

/** Flash 数据按 32 位访问时的字节长度 */
#define PARAM_WORD_SIZE             4U

/** Flash 参数数据有效性标识，用于识别已初始化的参数 */
#define DATA_VALID_ID               (0x4829DAD1)


/* Typedef -----------------------------------------------------------------------------------------------------------*/

/**
 * @brief Flash 参数分区编号
 * @note  用于索引 g_param_list 中对应的参数分区信息。
 */
typedef enum
{
    PARAM_ID_FAULT = 0,          /**< 故障记录参数分区 */
    PARAM_ID_SYSTEM,             /**< 系统运行参数分区 */
    PARAM_ID_SOC,                /**< 电池 SOC 相关参数分区 */
    PARAM_ID_OTA,                /**< OTA 升级状态参数分区 */
    PARAM_ID_MAX                 /**< 参数分区总数 */
} param_id_t;


/**
 * @brief Flash 页数据访问联合体
 * @note  提供字节和 32 位字两种访问方式，共用同一块存储空间。
 */
typedef union
{
    uint8_t  byte[PARAM_PAGE_SIZE];                   /**< 按字节访问 Flash 页数据 */
    uint32_t word[PARAM_PAGE_SIZE / PARAM_WORD_SIZE]; /**< 按 32 位字访问 Flash 页数据 */
} flash_page_t;


/**
 * @brief Flash 参数分区配置信息
 * @note  描述参数分区的起始地址、占用页数及存储容量。
 */
typedef struct
{
    uint32_t addr;               /**< 参数分区 Flash 起始地址 */
    uint32_t pageNum;            /**< 参数分区占用的 Flash 页数 */
    uint32_t size;               /**< 参数分区存储容量，单位 Byte */
} param_list_t;


/**
 * @brief OTA 升级状态枚举
 * @note  用于记录 OTA 升级过程中的运行状态。
 */
typedef enum
{
    OTC_STATUS_START = 0,        /**< OTA 升级开始 */
    OTC_STATUS_PROGRAMMING,      /**< OTA 固件写入中 */
    OTC_STATUS_SUCCESS           /**< OTA 升级成功 */
} otc_status_enum_t;


/* External variables ------------------------------------------------------------------------------------------------*/

/** Flash 参数分区配置表，下标对应 param_id_t */
extern const param_list_t g_param_list[PARAM_ID_MAX];


/* Function declarations ---------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : param_Init
//  Description: 初始化 Flash 参数管理模块
//  Param      : None
//  Return     : None
//  Others     : 系统启动时调用，用于初始化参数管理相关状态
//  Author     : Leo Chen 2026-08-02
*******************************************************************************/
void param_Init(void);

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
void param_soc_read(uint8_t *buffer, uint8_t length);

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
void param_soc_save(const uint8_t *buffer, uint8_t length);

/*******************************************************************************
//  Function   : param_Restore
//  Description: 执行 Flash 参数恢复操作
//  Param      : value - 参数恢复控制值，具体含义由实现定义
//  Return     : None
//  Others     : 用于执行参数恢复，具体恢复范围及行为由 param.c 决定
//  Author     : Leo Chen 2026-08-02
*******************************************************************************/
void param_Restore(uint8_t value);


/*******************************************************************************
//  Function   : param_ota_save
//  Description: 保存 OTA 升级状态
//  Param      : status - 待保存的 OTA 状态
//  Return     : None
//  Others     : 将升级状态交由参数管理模块保存，具体存储方式由实现决定
//  Author     : Leo Chen 2026-08-02
*******************************************************************************/
void param_ota_save(otc_status_enum_t status);


/*******************************************************************************
//  Function   : param_ota_state
//  Description: 获取当前保存的 OTA 升级状态
//  Param      : None
//  Return     : 当前 OTA 升级状态
//  Others     : 返回 otc_status_enum_t 类型的升级状态
//  Author     : Leo Chen 2026-08-02
*******************************************************************************/
otc_status_enum_t param_ota_state(void);


#endif /* _PARAM_H_ */
