/**
 ***********************************************************************************************************************
 * @file    bms_ctrl.c
 * @brief   BMS 电池电压、电流采集及充放电状态管理
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_ctrl.h"
#include "bms_config.h"
#include "bms_status.h"
#include "bsp_adc.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

static bms_battery_data_t s_batteryData;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : sample_accum_init
//  Description: 初始化 ADC 双缓冲累计采样器
//  Param      : acc - 累计采样器
//  Return     : None
//  Others     : 清空两个缓冲区并将当前写缓冲区设置为 buf[0]
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void sample_accum_init(sample_accum_t *acc)
{
    uint8_t i;

    for (i = 0U; i < 2U; i++)
    {
        acc->buf[i].sum1     = 0U;
        acc->buf[i].sum2     = 0U;
        acc->buf[i].count    = 0U;
        acc->buf[i].complete = false;
    }

    acc->write_index = 0U;
}


/*******************************************************************************
//  Function   : sample_accum_push
//  Description: 向当前写缓冲区加入一次双通道 ADC 采样值
//  Param      : acc    - 累计采样器
//               value1 - 第一通道采样值
//               value2 - 第二通道采样值
//  Return     : None
//  Others     : 累计达到 SAMPLE_ACCUM_COUNT 后切换到另一缓冲区，适合在采样中断中调用
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void sample_accum_push(uint32_t value1,uint32_t value2)
{
    sample_accum_buf_t *buf;
    sample_accum_t     *acc = &s_batteryData.accum;
    buf = &acc->buf[acc->write_index];

    /* 缓冲区尚未被主循环处理时禁止覆盖。 */
    if (buf->complete)
    {
        return;
    }

    buf->sum1 += value1;
    buf->sum2 += value2;
    buf->count++;

    if (buf->count >= SAMPLE_ACCUM_COUNT)
    {
        buf->complete = true;

        /* 切换到另一个缓冲区继续采样。 */
        acc->write_index ^= 1U;
        buf = &acc->buf[acc->write_index];

        /* 仅清空已被处理完成的缓冲区。 */
        if (!buf->complete)
        {
            buf->sum1  = 0U;
            buf->sum2  = 0U;
            buf->count = 0U;
        }
    }
}


/*******************************************************************************
//  Function   : sample_accum_get
//  Description: 获取一组已完成的累计采样结果
//  Param      : acc  - 累计采样器
//               sum1 - 第一通道累计值输出
//               sum2 - 第二通道累计值输出
//  Return     : true：存在完成数据；false：暂无完成数据
//  Others     : 读取完成后自动释放对应缓冲区供中断再次写入
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool sample_accum_get(uint32_t *sum1,uint32_t *sum2)
{
    uint8_t             read_index;
    sample_accum_t     *acc = &s_batteryData.accum;
    /* 当前 write_index 正在写入，因此优先读取另一个缓冲区。 */
    read_index = acc->write_index ^ 1U;

    if (!acc->buf[read_index].complete)
    {
        return false;
    }

    *sum1 = acc->buf[read_index].sum1;
    *sum2 = acc->buf[read_index].sum2;

    /* 释放读取完成的缓冲区。 */
    acc->buf[read_index].sum1     = 0U;
    acc->buf[read_index].sum2     = 0U;
    acc->buf[read_index].count    = 0U;
    acc->buf[read_index].complete = false;

    return true;
}


/*******************************************************************************
//  Function   : bms_battery_voltage
//  Description: 根据 VREFINT ADC 采样值计算电池电压
//  Param      : None
//  Return     : None
//  Others     : ADC 参考电压为 VCCA，本板 VCCA 与 VBAT 相连，因此通过内部 VREFINT
//               反推出 VCCA，即得到电池电压
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void bms_battery_voltage(uint32_t vrefAdc)
{
    /*
     * ADC_VREFINT = VREFINT / VCCA × ADC_FULL_SCALE
     *
     * 因此：
     *
     * VCCA = VREFINT × ADC_FULL_SCALE / ADC_VREFINT
     *
     * 本板 VCCA 与 VBAT 相连，所以 VCCA 即电池电压。
     */
    if ((vrefAdc == 0U) || (vrefAdc > BMS_BATTERY_ADC_FULL_SCALE))
    {
        s_batteryData.voltageV = 0.0;
        return;
    }

    s_batteryData.voltageV =
        BMS_BATTERY_VREFINT_V *
        (double)BMS_BATTERY_ADC_FULL_SCALE /
        (double)vrefAdc;
}


/*******************************************************************************
//  Function   : bms_battery_current
//  Description: 根据 PGA2 ADC 采样值计算电池充放电电流
//  Param      : None
//  Return     : None
//  Others     : PGA2 放大 R18 两端压差，零电流输出为 VCCA 的固定比例；
//               正电流表示放电，负电流表示充电
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void bms_battery_current(uint32_t pgaAdc)
{
    /* ADC/PGA 尚未有效或输出饱和时不进行电流换算。 */
    if ((pgaAdc == 0U) || (pgaAdc >= BMS_BATTERY_ADC_FULL_SCALE) || (s_batteryData.voltageV <= 0.0))
    {
        s_batteryData.currentA = 0.0;
        return;
    }

    /*
     * PGA 输出：
     *
     * V_PGA = ADC_PGA / ADC_FULL_SCALE × VCCA
     *
     * 零电流基准：
     *
     * V_ZERO = ZERO_RATIO × VCCA
     *
     * 分流电阻压差：
     *
     * V_SHUNT = (V_PGA - V_ZERO) / PGA_GAIN
     *
     * 电池电流：
     *
     * I = V_SHUNT / R_SHUNT
     *
     * 合并后：
     *
     * I =
     * (ADC_PGA / ADC_FULL_SCALE - ZERO_RATIO)
     * × VCCA
     * / (PGA_GAIN × R_SHUNT)
     */
    s_batteryData.currentA =
        (((double)pgaAdc / (double)BMS_BATTERY_ADC_FULL_SCALE) -
         BMS_BATTERY_PGA_ZERO_RATIO) *
        s_batteryData.voltageV /
        (BMS_BATTERY_PGA_GAIN * BMS_BATTERY_SHUNT_OHM) *
        BMS_BATTERY_CURRENT_SIGN;
}


/*******************************************************************************
//  Function   : bms_battery_state
//  Description: 根据电池电流判断当前充放电状态
//  Param      : None
//  Return     : None
//  Others     : 使用空闲电流阈值形成零点死区，避免 ADC/PGA 噪声导致状态频繁切换
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void bms_battery_state(void)
{
    if (s_batteryData.currentA > 0.2)
    {
        s_batteryData.state = BMS_POWER_DISCHARGING;
    }
    else if (s_batteryData.currentA < -0.2)
    {
        s_batteryData.state = BMS_POWER_CHARGING;
    }
    else
    {
        s_batteryData.state = BMS_POWER_IDLE;
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/
/*******************************************************************************
//  Function   : bms_battery_state
//  Description: 根据电池电流判断当前充放电状态
//  Param      : None
//  Return     : None
//  Others     : 使用空闲电流阈值形成零点死区，避免 ADC/PGA 噪声导致状态频繁切换
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void bms_ctrl_Init(void)
{
    sample_accum_init(&s_batteryData.accum);
}

/*******************************************************************************
//  Function   : bms_battery_state
//  Description: 根据电池电流判断当前充放电状态
//  Param      : None
//  Return     : None
//  Others     : 使用空闲电流阈值形成零点死区，避免 ADC/PGA 噪声导致状态频繁切换
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void bms_ctrl_run(void)
{
    uint32_t vbat_adc,curr_adc;

    //检测
    bms_status_check(s_batteryData.state);

    // adc 采样次数 64ms
    if(sample_accum_get(&vbat_adc,&curr_adc))
    {
        vbat_adc /= SAMPLE_ACCUM_COUNT;
        curr_adc /= SAMPLE_ACCUM_COUNT;
        
        //获取电池 - 电压&电流状态
        bms_battery_voltage(vbat_adc);
        bms_battery_current(curr_adc);
        bms_battery_state();
    }
	
	
}