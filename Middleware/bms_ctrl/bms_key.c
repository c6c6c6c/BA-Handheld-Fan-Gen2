/**
 ***********************************************************************************************************************
 * @file    bms_key.c
 * @brief   IP5326 KEY 非阻塞控制，实现短按、长按、双击及保护间隔
 * @author  Leo Chen
 * @date    2026-10-14
 ***********************************************************************************************************************
 */

#include "bms_key.h"
#include "timer_base.h"
#include <string.h>


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** 各 KEY 命令对应的阶段时间配置 */
const bms_key_process_t s_process[BMS_KEY_NUM] =
{
    [BMS_KEY_WAKEUP]       = { 1U, (const uint16_t[]){1000U}},
    [BMS_KEY_STANDBY]      = { 1U, (const uint16_t[]){2100U}},
    [BMS_KEY_LIGHT_TOGGLE] = { 3U, (const uint16_t[]){150U, 200U, 150U}}
};

/** KEY 状态机运行状态 */
bms_key_status_t s_bms_key = {0};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_key_SetPressed
//  Description: 设置 IP5326 KEY 引脚状态
//  Param      : pressed - true：模拟按下；false：模拟释放
//  Return     : None
//  Others     : 实际 GPIO 有效电平由 bms_config.h 中的 KEY 控制宏决定
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
static void bms_key_SetPressed(bool pressed)
{
    if (pressed)
    {
        BMS_IO_KEY_PRESS();
    }
    else
    {
        BMS_IO_KEY_RELEASE();
    }

    s_bms_key.pressed = pressed;
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_key_Request
//  Description: 请求执行一次 IP5326 KEY 控制命令
//  Param      : command - KEY 控制命令
//  Return     : None
//  Others     : 仅更新目标命令，状态机在空闲状态下开始执行
//
//               BMS_KEY_WAKEUP
//                   模拟一次短按，用于唤醒电量显示及升压输出
//
//               BMS_KEY_STANDBY
//                   模拟一次长按，用于进入待机状态
//
//               BMS_KEY_LIGHT_TOGGLE
//                   模拟连续两次短按，用于切换照明灯状态
//
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_Request(bms_key_command_t command)
{
    s_bms_key.target = command;
}


/*******************************************************************************
//  Function   : bms_key_RequestForce
//  Description: 强制请求执行一次 IP5326 KEY 控制命令
//  Param      : command - KEY 控制命令
//  Return     : None
//  Others     : 先进入 Guard 状态，等待保护时间结束后再执行新的目标命令
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_RequestForce(bms_key_command_t command)
{
    if (s_bms_key.execute < BMS_KEY_NUM)
    {
        s_bms_key.execute    = BMS_KEY_GUARD;
        s_bms_key.step_times = 0U;
    }

    s_bms_key.target     = command;
}


/*******************************************************************************
//  Function   : bms_key_Init
//  Description: 初始化 IP5326 KEY 非阻塞状态机
//  Param      : None
//  Return     : None
//  Others     : 状态机初始化为空闲状态，并清零阶段索引及阶段计时
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_Init(void)
{
    s_bms_key.target       = BMS_KEY_LIGHT_IDEL;
    s_bms_key.execute      = BMS_KEY_LIGHT_IDEL;
    s_bms_key.lastComplete = BMS_KEY_LIGHT_IDEL;
    s_bms_key.step         = 0U;
    s_bms_key.step_times   = 0U;
}


/*******************************************************************************
//  Function   : bms_key_1ms_task
//  Description: 周期推进 IP5326 KEY 非阻塞状态机
//  Param      : None
//  Return     : None
//  Others     : 建议每 1 ms 调用一次；每个命令按照 s_process[] 中配置的 times[]
//               顺序执行，各阶段到时后翻转 KEY 电平，全部阶段完成后进入 Guard
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_1ms_task(void)
{
    static bool io_pressed = BMS_KEY_IDLE_IO;

    /* 当前阶段运行时间累计。 */
    s_bms_key.step_times++;

    switch (s_bms_key.execute)
    {
        case BMS_KEY_LIGHT_IDEL:
            io_pressed = BMS_KEY_IDLE_IO;

            /* 存在有效目标命令时开始执行。 */
            if (s_bms_key.target < BMS_KEY_NUM)
            {
                s_bms_key.execute    = s_bms_key.target;
                s_bms_key.target     = BMS_KEY_LIGHT_IDEL;
                s_bms_key.step       = 0U;
                s_bms_key.step_times = 0U;
                io_pressed          = !BMS_KEY_IDLE_IO;
            }
            break;

        case BMS_KEY_GUARD:
            /* Guard 阶段保持 KEY 释放状态。 */
            io_pressed = BMS_KEY_IDLE_IO;

            if (s_bms_key.step_times >= BMS_KEY_GUARD_TIME_MS)
            {
                s_bms_key.execute = BMS_KEY_LIGHT_IDEL;
            }
            break;

        case BMS_KEY_NUM:
            /* 非法执行状态，恢复为空闲状态。 */
            io_pressed       = BMS_KEY_IDLE_IO;
            s_bms_key.execute = BMS_KEY_LIGHT_IDEL;
            break;

        default:
            /*
             * 按 times[] 顺序执行各阶段。
             * 每达到当前阶段配置时间后，进入下一阶段并翻转 KEY 电平。
             */
            if (s_bms_key.step < s_process[s_bms_key.execute].count)
            {
                if (s_bms_key.step_times >= s_process[s_bms_key.execute].times[s_bms_key.step])
                {
                    s_bms_key.step++;
                    s_bms_key.step_times = 0U;
                    io_pressed          = !io_pressed;
                }
            }
            else
            {
                /* 所有阶段执行完成，进入 Guard 状态。 */
                s_bms_key.lastComplete = s_bms_key.execute;
                s_bms_key.execute      = BMS_KEY_GUARD;
                s_bms_key.step_times   = 0U;
                io_pressed            = BMS_KEY_IDLE_IO;
            }
            break;
    }

    /* 根据状态机计算结果更新实际 KEY IO。 */
    bms_key_SetPressed(io_pressed);
}


/*******************************************************************************
//  Function   : bms_key_GetStatus
//  Description: 获取 KEY 状态机当前运行状态
//  Param      : None
//  Return     : KEY 状态只读指针
//  Others     : 仅表示 MCU 模拟 KEY 的执行状态，不代表 IP5326 实际内部工作状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_key_status_t *bms_key_GetStatus(void)
{
    return &s_bms_key;
}
