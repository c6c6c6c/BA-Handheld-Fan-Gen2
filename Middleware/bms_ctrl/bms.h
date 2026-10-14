/**
 * @file    bms.h
 * @brief   BMS 模块统一包含入口；所有使能开关与参数位于 bms_config.h
 * @date    2026-10-14
 * @note    配置头不反向包含业务头文件，避免循环依赖。
 */
#ifndef _BMS_H_
#define _BMS_H_
#include "bms_config.h"
#if BMS_MODULE_KEY_ENABLE
#include "bms_key.h"
#endif
#if BMS_MODULE_STATUS_ENABLE
#include "bms_status.h"
#endif
#if BMS_MODULE_PROTECT_ENABLE
#include "bms_protect.h"
#endif
#if BMS_MODULE_SOC_ENABLE
#include "bms_soc.h"
#endif
#if BMS_MODULE_CTRL_ENABLE
#include "bms_ctrl.h"
#endif
#endif /* _BMS_H_ */
