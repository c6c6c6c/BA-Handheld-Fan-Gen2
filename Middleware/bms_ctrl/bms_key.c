/**
 ***********************************************************************************************************************
 * @file    bms_key.c
 * @brief   IP5326 KEY 独立非阻塞状态机，支持短按、长按、双击及 Guard
 * @author  Leo Chen
 * @date    2026-10-14
 ***********************************************************************************************************************
 */
#include "bms_key.h"

#if BMS_MODULE_KEY_ENABLE

#include <string.h>




static bms_key_status_t s_status;
static bool s_initialized;
static uint16_t s_remainingMs;
static bms_key_phase_t s_phase;


/*******************************************************************************
//  Function   : bms_key_SetPressed
//  Description: 按配置宏驱动 IP5326 KEY，并记录模拟按下状态
//  Param      : pressed - true：按下；false：释放
//  Return     : None
//  Others     : 实际极性由 bms_config.h 适配
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

    s_status.pressed = pressed;
}


/*******************************************************************************
//  Function   : bms_key_Init
//  Description: 初始化 IP5326 KEY 状态机
//  Param      : None
//  Return     : true：成功；false：KEY GPIO 未正确配置
//  Others     : KEY 默认处于释放状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_key_Init(void)
{
    s_initialized = false;

    memset(&s_status, 0, sizeof(s_status));

    s_phase       = BMS_KEY_IDLE;
    s_remainingMs = 0U;

    if (!BMS_IO_KEY_IS_READY())
    {
        return false;
    }

    /* KEY 必须默认释放 */
    bms_key_SetPressed(false);

    /*
     * 默认允许控制。
     * 业务层可通过 bms_key_SetAllowed() 动态禁止非待机命令。
     */
    s_status.allowed = true;

    /*
     * command 这里只设置一个合法初值。
     */
    s_status.command = BMS_KEY_WAKEUP;

    s_status.busy = false;

    s_initialized = true;

    return true;
}


/*******************************************************************************
//  Function   : bms_key_Cancel
//  Description: 取消当前 KEY 动作并进入 Guard
//  Param      : None
//  Return     : None
//  Others     : 中途释放 KEY 后，IP5326 可能将已经达到有效时间的脉冲解释为短按
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_Cancel(void)
{
    if (!s_initialized)
    {
        return;
    }

    bms_key_SetPressed(false);

    s_phase       = BMS_KEY_GUARD;
    s_remainingMs = BMS_CTRL_KEY_GUARD_MS;
    s_status.busy = true;
}


/*******************************************************************************
//  Function   : bms_key_SetAllowed
//  Description: 更新非待机命令权限
//  Param      : allowed - true：允许；false：禁止
//  Return     : None
//  Others     : STANDBY 始终允许，便于系统紧急进入待机
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_SetAllowed(bool allowed)
{
    s_status.allowed = allowed;

    /*
     * 如果当前正在执行非 STANDBY 命令，并且上层撤销权限，
     * 则立即释放 KEY。
     *
     * STANDBY 不受 allowed 限制。
     */
    if (!allowed &&
        (s_status.command != BMS_KEY_STANDBY) &&
        ((s_phase == BMS_KEY_FIRST_PRESS) ||
         (s_phase == BMS_KEY_DOUBLE_GAP)  ||
         (s_phase == BMS_KEY_SECOND_PRESS)))
    {
        bms_key_Cancel();
    }
}


/*******************************************************************************
//  Function   : bms_key_Request
//  Description: 请求一次 IP5326 KEY 动作
//  Param      : command - KEY 控制命令
//  Return     : true：接受；false：状态机忙或当前状态不允许
//
//  Command:
//      BMS_KEY_WAKEUP
//          一次有效短按 (>100 ms, <2 s)
//          用于开启电量显示和升压输出
//
//      BMS_KEY_STANDBY
//          一次有效长按 (>2 s)
//          用于进入待机
//
//      BMS_KEY_LIGHT_TOGGLE
//          1 s 内连续两次有效短按
//          用于打开/关闭照明灯
//
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
bool bms_key_Request(bms_key_command_t command)
{
    /* 状态机未初始化 */
    if (!s_initialized)
    {
        return false;
    }

    /* 上一个 KEY 动作尚未结束 */
    if (s_phase != BMS_KEY_IDLE)
    {
        return false;
    }

    /* 参数合法性检查 */
    if ((unsigned int)command > (unsigned int)BMS_KEY_LIGHT_TOGGLE)
    {
        return false;
    }

    /* 除 STANDBY 外，其余动作需要业务层允许 */
    if ((command != BMS_KEY_STANDBY) && !s_status.allowed)
    {
        return false;
    }

    s_status.command = command;
    s_status.busy    = true;

    /* 三种命令均从第一次 KEY 按下开始 */
    s_phase = BMS_KEY_FIRST_PRESS;

    switch (command)
    {
        case BMS_KEY_STANDBY:
            /*
             * IP5326:
             * 按键时间 > 2 s 为长按。
             *
             * 推荐 2100 ms，避免刚好卡在 2000 ms 临界点。
             */
            s_remainingMs = BMS_CTRL_KEY_LONG_MS;
            break;

        case BMS_KEY_WAKEUP:
        case BMS_KEY_LIGHT_TOGGLE:
        default:
            /*
             * IP5326:
             * >100 ms 且 <2 s 为有效短按。
             *
             * LIGHT_TOGGLE 第一次点击同样是短按。
             */
            s_remainingMs = BMS_CTRL_KEY_SHORT_MS;
            break;
    }

    bms_key_SetPressed(true);

    return true;
}


/*******************************************************************************
//  Function   : bms_key_1ms_task
//  Description: 推进 IP5326 KEY 非阻塞状态机
//  Param      : None
//  Return     : None
//  Others     : 必须保持约 1 ms 调度，KEY 动作结束前不可停止调用
//
//  Timing:
//
//  WAKEUP:
//      KEY ______|----------|________________
//                  SHORT
//
//  STANDBY:
//      KEY ______|--------------------------|_
//                    > 2000 ms
//
//  LIGHT_TOGGLE:
//      KEY ______|----|____|----|____________
//                  S    GAP  S
//
//                <--------->
//                  < 1 s
//
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
void bms_key_1ms_task(void)
{
    if (!s_initialized)
    {
        return;
    }

    if (s_phase == BMS_KEY_IDLE)
    {
        return;
    }

    /*
     * 当前阶段计时。
     */
    if (s_remainingMs > 0U)
    {
        s_remainingMs--;
    }

    /*
     * 当前阶段尚未结束。
     */
    if (s_remainingMs > 0U)
    {
        return;
    }

    switch (s_phase)
    {
        /*
         * 第一次按键结束。
         *
         * WAKEUP:
         *      一次短按结束，进入 Guard。
         *
         * STANDBY:
         *      一次长按结束，进入 Guard。
         *
         * LIGHT_TOGGLE:
         *      第一次短按结束，释放 KEY，
         *      等待一小段间隔后进行第二次短按。
         */
        case BMS_KEY_FIRST_PRESS:

            bms_key_SetPressed(false);

            if (s_status.command == BMS_KEY_LIGHT_TOGGLE)
            {
                s_phase = BMS_KEY_DOUBLE_GAP;
                s_remainingMs = BMS_CTRL_KEY_DOUBLE_GAP_MS;
            }
            else
            {
                s_phase = BMS_KEY_GUARD;
                s_remainingMs = BMS_CTRL_KEY_GUARD_MS;
            }

            break;


        /* 双击两次短按之间的释放阶段结束。 开始第二次短按。 */
        case BMS_KEY_DOUBLE_GAP:

            bms_key_SetPressed(true);

            s_phase       = BMS_KEY_SECOND_PRESS;
            s_remainingMs = BMS_CTRL_KEY_SHORT_MS;

            break;
        /* 双击第二次短按完成。 */
        case BMS_KEY_SECOND_PRESS:

            bms_key_SetPressed(false);

            s_phase       = BMS_KEY_GUARD;
            s_remainingMs = BMS_CTRL_KEY_GUARD_MS;

            break;
        /* Guard 完成: KEY 保证释放，并返回 IDLE */
        case BMS_KEY_GUARD:

            bms_key_SetPressed(false);

            s_phase       = BMS_KEY_IDLE;
            s_remainingMs = 0U;

            break;
        /* 状态异常时强制释放 KEY。 */
        default:

            bms_key_SetPressed(false);

            s_phase       = BMS_KEY_IDLE;
            s_remainingMs = 0U;

            break;
    }

    s_status.busy = (s_phase != BMS_KEY_IDLE);
}


/*******************************************************************************
//  Function   : bms_key_GetStatus
//  Description: 获取 KEY 状态机运行状态
//  Param      : None
//  Return     : 状态只读指针
//  Others     : 此状态仅表示 MCU 模拟 KEY 的状态，不代表 IP5326 实际工作状态
//  Author     : Chen Liang 2026-10-14
*******************************************************************************/
const bms_key_status_t *bms_key_GetStatus(void)
{
    return &s_status;
}

#endif /* BMS_MODULE_KEY_ENABLE */
