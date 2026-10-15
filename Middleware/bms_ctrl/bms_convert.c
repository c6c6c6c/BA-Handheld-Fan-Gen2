/**
 ***********************************************************************************************************************
 * @file    bms_convert.c
 * @brief   BMS ADC 累计采样及电池电压、电流、USB 输入电压换算
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#include "bms_convert.h"
#include "bms_config.h"


/* Variables ---------------------------------------------------------------------------------------------------------*/

/** ADC 多缓冲累计采样器 */
static sample_accum_t adc_accum;


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : sample_accum_init
//  Description: 初始化 ADC 多缓冲累计采样器
//  Param      : None
//  Return     : None
//  Others     : 清空所有累计缓冲区，并将读写索引复位到第 0 组
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void sample_accum_init(void)
{
    uint8_t i, j;

    for (i = 0U; i < BMS_ADC_NUM; i++)
    {
        for (j = 0U; j < SAMPLE_SUM_COUNT; j++)
        {
            adc_accum.buf[i].sum[j] = 0U;
        }

        adc_accum.buf[i].count    = 0U;
        adc_accum.buf[i].complete = false;
    }

    adc_accum.write_idx = 0U;
    adc_accum.read_idx  = 0U;
}


/*******************************************************************************
//  Function   : sample_accum_push
//  Description: 向当前写缓冲区加入一次多通道 ADC 采样值
//  Param      : adcValue - ADC 采样值数组，长度至少为 BMS_ADC_NUM
//  Return     : None
//  Others     : 累计达到 SAMPLE_ACCUM_COUNT 后标记当前缓冲区完成，并切换下一写缓冲区；
//               适合在 ADC/DMA 采样完成中断中调用
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void sample_accum_push(uint32_t *adcValue)
{
    sample_accum_buf_t *buf;
    uint8_t i;

    /* 输入参数无效时不进行累计。 */
    if (adcValue == NULL)
    {
        return;
    }

    buf = &adc_accum.buf[adc_accum.write_idx];

    /* 当前缓冲区尚未被读取时禁止覆盖。 */
    if (buf->complete)
    {
        return;
    }

    /* 累计各 ADC 通道采样值。 */
    for (i = 0U; i < BMS_ADC_NUM; i++)
    {
        buf->sum[i] += adcValue[i];
    }

    buf->count++;

    /* 达到指定累计次数后发布当前缓冲区。 */
    if (buf->count >= SAMPLE_ACCUM_COUNT)
    {
        buf->complete = true;

        /* 循环切换到下一写缓冲区。 */
        adc_accum.write_idx++;

        if (adc_accum.write_idx >= SAMPLE_SUM_COUNT)
        {
            adc_accum.write_idx = 0U;
        }
    }
}


/*******************************************************************************
//  Function   : sample_accum_get
//  Description: 获取一组已完成的 ADC 平均采样结果
//  Param      : adcValue - ADC 平均值输出数组，长度至少为 BMS_ADC_NUM
//  Return     : true：成功获取一组数据；false：暂无完成数据或参数异常
//  Others     : 读取完成后释放当前缓冲区，并切换到下一读缓冲区
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool sample_accum_get(double *adcValue)
{
    sample_accum_buf_t *buf;
    uint8_t i;

    /* 输出参数无效时不读取数据。 */
    if (adcValue == NULL)
    {
        return false;
    }

    buf = &adc_accum.buf[adc_accum.read_idx];

    /* 当前读缓冲区尚未累计完成。 */
    if (!buf->complete)
    {
        return false;
    }

    /* 完成缓冲区的采样次数应等于配置的累计次数。 */
    if (buf->count != SAMPLE_ACCUM_COUNT)
    {
        buf->count    = 0U;
        buf->complete = false;
        return false;
    }

    /* 计算各 ADC 通道平均值，并清空累计值。 */
    for (i = 0U; i < BMS_ADC_NUM; i++)
    {
        adcValue[i] = (double)buf->sum[i] / (double)SAMPLE_ACCUM_COUNT;
        buf->sum[i] = 0U;
    }

    buf->count    = 0U;
    buf->complete = false;

    /* 循环切换到下一读缓冲区。 */
    adc_accum.read_idx++;

    if (adc_accum.read_idx >= SAMPLE_SUM_COUNT)
    {
        adc_accum.read_idx = 0U;
    }

    return true;
}


/*******************************************************************************
//  Function   : bms_battery_voltage
//  Description: 根据 VREFINT ADC 采样值计算电池电压
//  Param      : vrefAdc - VREFINT ADC 采样值
//  Return     : 电池电压，单位 V；采样无效时返回 0
//  Others     : ADC 参考电压为 VCCA，本板 VCCA 与 VBAT 相连，因此通过内部 VREFINT
//               反推出 VCCA，即得到电池电压
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_battery_voltage(uint32_t vrefAdc)
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
        return 0.0;
    }

    return BMS_BATTERY_VREFINT_V *
           (double)BMS_BATTERY_ADC_FULL_SCALE /
           (double)vrefAdc;
}


/*******************************************************************************
//  Function   : bms_battery_current
//  Description: 根据 PGA2 ADC 采样值计算电池充放电电流
//  Param      : pgaAdc - PGA2 输出 ADC 采样值
//               vbat   - 当前电池电压，单位 V
//  Return     : 电池电流，单位 A；正值表示放电，负值表示充电；采样无效时返回 0
//  Others     : PGA2 放大分流电阻 R18 两端压差，零电流输出为 VCCA 的固定比例
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_battery_current(uint32_t pgaAdc, double vbat)
{
    /* ADC/PGA 尚未有效、输出饱和或电池电压无效时不进行换算。 */
    if ((pgaAdc == 0U) ||
        (pgaAdc >= BMS_BATTERY_ADC_FULL_SCALE) ||
        (vbat <= 0.0))
    {
        return 0.0;
    }

    /*
     * V_PGA   = ADC_PGA / ADC_FULL_SCALE × VCCA
     * V_ZERO  = ZERO_RATIO × VCCA
     * V_SHUNT = (V_PGA - V_ZERO) / PGA_GAIN
     * I       = V_SHUNT / R_SHUNT
     */
    return (((double)pgaAdc / (double)BMS_BATTERY_ADC_FULL_SCALE) -
            BMS_BATTERY_PGA_ZERO_RATIO) *
           vbat /
           (BMS_BATTERY_PGA_GAIN * BMS_BATTERY_SHUNT_OHM) *
           BMS_BATTERY_CURRENT_SIGN;
}


/*******************************************************************************
//  Function   : bms_usb_in_voltage
//  Description: 根据 USB ADC 采样值计算 VBUS 输入电压
//  Param      : usbAdc - USB ADC 采样值
//  Return     : USB VBUS 电压，单位 V；采样无效时返回 0
//  Others     : 根据 ADC 参考电压及外部分压比例恢复实际 VBUS 电压
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
double bms_usb_in_voltage(uint32_t usbAdc)
{
    double adcVoltage;

    /* ADC 尚未有效或输出达到满量程时不进行换算。 */
    if ((usbAdc == 0U) || (usbAdc >= BMS_BATTERY_ADC_FULL_SCALE))
    {
        return 0.0;
    }

    /* 将 ADC 码值换算为 ADC 输入端实际电压。 */
    adcVoltage =
        ((double)usbAdc * BMS_USB_ADC_REF_VOLTAGE) /
        (double)BMS_BATTERY_ADC_FULL_SCALE;

    /* 根据硬件分压比例恢复实际 VBUS 电压。 */
    return adcVoltage * BMS_USB_ADC_DIV_RATIO;
}


/*******************************************************************************
//  Function   : bms_battery_state
//  Description: 根据电池电流判断当前充放电状态
//  Param      : currentA - 当前电池电流，单位 A
//  Return     : 当前电池工作状态
//  Others     : 使用 ±0.2A 作为空闲判断死区，避免 PGA/ADC 零点噪声导致状态频繁切换
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
battery_state_t bms_battery_state(double currentA)
{
    if (currentA > 0.2)
    {
        return BAT_STATE_DISCHARGING;
    }
    else if (currentA < -0.2)
    {
        return BAT_STATE_CHARGING;
    }
    else
    {
        return BAT_STATE_IDLE;
    }
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : bms_convert_Init
//  Description: 初始化 BMS ADC 数据换算模块
//  Param      : None
//  Return     : None
//  Others     : 初始化 ADC 累计采样器并复位所有读写缓冲区
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
void bms_convert_Init(void)
{
    sample_accum_init();
}
