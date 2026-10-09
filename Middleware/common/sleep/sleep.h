/**
 ***********************************************************************************************************************
 * @file    sleep.h
 * @brief
 * @author  Su Jiawei
 * @version 1.0.0
 * @date    2025-09-03
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _SLEEP_H_
#define _SLEEP_H_

#include "main.h"

/* Macro -------------------------------------------------------------------------------------------------------------*/

/* Typedef -----------------------------------------------------------------------------------------------------------*/

typedef enum
{
    EXIT_SLEEP,
    ENTER_SLEEP,
    SLEEP_STATE_MAX
} sleep_state_t;


typedef struct 
{
    sleep_state_t state;
    uint32_t      sleepCnt;
    uint32_t      bmsCnt;
    uint8_t       quickSleepflg;
}sleep_t;

/* Function declarations ---------------------------------------------------------------------------------------------*/

void sleep_SetState(sleep_state_t state);
sleep_state_t sleep_getState(void);
void sleep_ClrCnt(void);
void sleep_QuickSleep(void);
void sleep_ClrQuickFlg(void);
void bms_connect_task(void);
void system_sleep_task(void);
void sleep_1ms_task(void);
bool sleep_close_attachment(void);
#endif
