/** 
 *********************************************************************************************************************** 
 * @file    param.h 
 * @brief   Flash参数管理模块接口定义，包括按键记录、故障记录、产品信息及OTA状态参数
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
/* Flash参数功能使能配置 */     
#ifdef PROJECT_APP 
#define PARAM_KEY_EN               1 
#define PARAM_FAULT_EN             1 
#define PARAM_PRODUCT_EN           1 
#endif 
#define PARAM_UPDATE_EN            1 
 
 
/* Flash基础参数 */ 
#define FLASH_BASE_ADDR            FLASH_BASE 
#define FLASH_END_ADDR             FLASH_END 
 
#define PARAM_PAGE_SIZE            FLASH_PAGE_SIZE 
#define PARAM_WORD_SIZE            4U 
 
/* Flash数据有效性标识 */ 
#define DATA_VALID_ID              (0x4829DAD1) 
 
/*********************************************** 
* Enum Description: Flash参数区域编号 
***********************************************/ 
typedef enum 
{ 
    #if PARAM_KEY_EN 
        PARAM_ID_KEY = 0, 
    #endif 
    #if PARAM_FAULT_EN 
        PARAM_ID_FAULT, 
    #endif 
    #if PARAM_PRODUCT_EN 
        PARAM_ID_PRODUCT, 
    #endif 
    #if PARAM_UPDATE_EN 
        PARAM_ID_OTA, 
    #endif 
    PARAM_ID_MAX 
} param_id_t; 
 
/*********************************************** 
* Union Description: Flash页数据访问结构，支持字节和32位字访问 
***********************************************/ 
typedef union 
{ 
    uint8_t  byte[PARAM_PAGE_SIZE]; 
    uint32_t word[PARAM_PAGE_SIZE / PARAM_WORD_SIZE]; 
} flash_page_t; 
 
/*********************************************** 
* Struct Description: Flash参数分区信息 
***********************************************/ 
typedef struct  
{ 
    uint32_t addr; 
    uint32_t pageNum; 
    uint32_t size; 
}param_list_t; 
 
#if PARAM_KEY_EN 
/*********************************************** 
* Struct Description: 按键累计计数掉电记录参数 
***********************************************/ 
#define BUTTON_RECORD_PAGE_NUM                8 // 按键记录占用的Flash页数 
#define BUTTON_RECORD_NUM                     4 // 单条记录包含的按键计数数量 
#define BUTTON_RECORD_HEAD                    1 // 单条记录头部数据数量，用于保存累计写入次数 
#define BUTTON_RECORD_ITEM                    (BUTTON_RECORD_NUM + BUTTON_RECORD_HEAD) 
#define BUTTON_RECORD_ITEM_SIZE               (PARAM_PAGE_SIZE / BUTTON_RECORD_ITEM / sizeof(uint32_t)) 
#define BUTTON_RECORD_ITEM_TOTAL              (BUTTON_RECORD_ITEM_SIZE * BUTTON_RECORD_PAGE_NUM) 
 
/* 根据记录索引计算所在Flash页 */ 
#define BUTTON_GET_PAGE_NUM(idx)              ((idx) / BUTTON_RECORD_ITEM_SIZE) 
 
/* 根据记录索引和数据偏移计算Flash存储偏移地址 */ 
#define BUTTON_GET_PAGE_OFFSET(idx, offset)                                                                 \
                                (BUTTON_GET_PAGE_NUM(idx) * PARAM_PAGE_SIZE +                               \
                                ((idx) % BUTTON_RECORD_ITEM_SIZE) * BUTTON_RECORD_ITEM * sizeof(uint32_t) + \
                                (offset) * sizeof(uint32_t)) 
 
 
/* 按键记录累计保存次数上限 */ 
#define BUTTON_VALUE_SAVE_TIMES_MAX  (10000u * BUTTON_RECORD_ITEM_TOTAL) 

/*********************************************** 
* Struct Description: 按键累计计数记录数据 
***********************************************/ 
typedef struct  
{ 
    uint32_t total; 
    uint32_t idx; 
    uint32_t data[BUTTON_RECORD_NUM]; 
}button_record_t; 
 
extern button_record_t param_button; 
 
void param_button_clear(void); 
void param_button_read(void); 
void param_button_save(void); 
uint32_t param_button_getCnt(uint8_t Item); 
#endif 
 
 
#if PARAM_FAULT_EN 
/*********************************************** 
* Struct Description: 故障掉电记录数据 
***********************************************/ 
#define FAULT_RECORD_NUMBER             (10) 
#define FAULT_RECORD_DATA_SIZE          (10 * sizeof(uint32_t)) 
#define COMMUNICATION_FAULT_CODE        (0x00008000) 
typedef struct 
{ 
    uint32_t cnt; 
	uint32_t idx; 
    uint32_t buf[FAULT_RECORD_NUMBER]; 
} fault_record_t; 
 
extern fault_record_t param_fault;    // 故障记录数据 
 
void param_fault_clear(void); 
void param_fault_read(void); 
void param_fault_save(uint32_t count, uint8_t *buffer, uint8_t length); 
#endif 
 
 
#if PARAM_PRODUCT_EN 
/*********************************************** 
* Struct Description: 产品信息掉电保存数据 
***********************************************/ 
#define INFO_BATTERY_SIZE               (23) 
#define INFO_DATE_SIZE                  (10) 
#define INFO_PRODUCT_SIZE               (19) 
#define INFO_SIZE                       (INFO_BATTERY_SIZE + INFO_DATE_SIZE + INFO_PRODUCT_SIZE) 
typedef struct 
{ 
    uint8_t buf[INFO_SIZE]; 
} product_info_t; 
 
extern product_info_t param_product;  // 产品信息数据 
 
void param_product_read(void); 
void param_product_save(uint8_t *buffer, uint8_t length); 
#endif 
 
 
/*********************************************** 
* Enum Description: OTA升级状态 
***********************************************/ 
typedef enum 
{ 
    OTC_STATUS_START, 
    OTC_STATUS_PROGRAMMING, 
    OTC_STATUS_SUCCESS, 
} otc_status_enum_t; 
 
void param_ota_save(otc_status_enum_t status); 
otc_status_enum_t param_ota_state(void);
 
 
/* Flash参数分区配置表 */ 
extern const param_list_t g_param_list[PARAM_ID_MAX]; 
/* Typedef -----------------------------------------------------------------------------------------------------------*/ 
 
 
/* Function declarations ---------------------------------------------------------------------------------------------*/ 
void param_Init(void); 
void param_Restore(uint8_t value); 
#endif 
