/**
 ***********************************************************************************************************************
 * @file    aip650e.c
 * @brief   AiP650E 两线通信及显示板驱动
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-10-15
 ***********************************************************************************************************************
 */

#include "aip650e.h"
#include "bsp_gpio.h"
#include <stddef.h>


/* Macro -------------------------------------------------------------------------------------------------------------*/

/** 系统控制寄存器命令 */
#define AIP650E_CMD_SYSTEM          0x48U

/** DIG1 显示 RAM 起始地址 */
#define AIP650E_ADDR_DIG1           0x68U

/** 相邻 DIG 显示 RAM 地址步长 */
#define AIP650E_ADDR_STEP           0x02U

/** 显示开启控制位 */
#define AIP650E_CONTROL_DISPLAY_ON  0x01U

/** 亮度控制位偏移 */
#define AIP650E_BRIGHTNESS_SHIFT    4U


/* Variables ---------------------------------------------------------------------------------------------------------*/

static bool s_initialized;                                  /**< 驱动初始化状态 */
static bool s_displayEnabled;                               /**< 当前显示开关状态 */
static uint8_t s_brightness = AIP650E_DEFAULT_BRIGHTNESS;   /**< 当前全局亮度等级 */


/* Constant ----------------------------------------------------------------------------------------------------------*/

/** 数字 0~9 对应段码，bit0~bit6 对应 A~G */
static const uint8_t s_numberSegments[10] =
{
    0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U,
    0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650e_Delay
//  Description: AiP650E 软件总线时序延时
//  Param      : None
//  Return     : None
//  Others     : 延时时间由 AIP650E_DELAY_CYCLES 和系统主频共同决定
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void aip650e_Delay(void)
{
    uint32_t count;

    for (count = 0U; count < AIP650E_DELAY_CYCLES; ++count)
    {
        __NOP();
    }
}


/*******************************************************************************
//  Function   : aip650e_SetClock
//  Description: 设置 AiP650E CLK 总线电平
//  Param      : high - true：释放总线为高；false：输出低电平
//  Return     : None
//  Others     : CLK 使用开漏输出，高电平由外部上拉电阻提供
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void aip650e_SetClock(bool high)
{
    if (high)
    {
        LL_GPIO_SetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN);
    }
}


/*******************************************************************************
//  Function   : aip650e_SetData
//  Description: 设置 AiP650E DIO 总线电平
//  Param      : high - true：释放总线为高；false：输出低电平
//  Return     : None
//  Others     : DIO 使用开漏输出，高电平由外部上拉电阻提供
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void aip650e_SetData(bool high)
{
    if (high)
    {
        LL_GPIO_SetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN);
    }
    else
    {
        LL_GPIO_ResetOutputPin(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN);
    }
}


/*******************************************************************************
//  Function   : aip650e_Start
//  Description: 产生 AiP650E 两线通信起始条件
//  Param      : None
//  Return     : None
//  Others     : CLK 为高电平期间 DIO 由高变低表示通信开始
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void aip650e_Start(void)
{
    aip650e_SetData(true);
    aip650e_SetClock(true);
    aip650e_Delay();

    aip650e_SetData(false);
    aip650e_Delay();

    aip650e_SetClock(false);
    aip650e_Delay();
}


/*******************************************************************************
//  Function   : aip650e_Stop
//  Description: 产生 AiP650E 两线通信停止条件
//  Param      : None
//  Return     : None
//  Others     : CLK 为高电平期间 DIO 由低变高表示通信结束
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static void aip650e_Stop(void)
{
    aip650e_SetData(false);
    aip650e_Delay();

    aip650e_SetClock(true);
    aip650e_Delay();

    aip650e_SetData(true);
    aip650e_Delay();
}


/*******************************************************************************
//  Function   : aip650e_WriteByte
//  Description: 向 AiP650E 写入一个字节
//  Param      : value - 待发送数据
//  Return     : true：收到 ACK；false：未收到 ACK
//  Others     : 数据高位先发送，第 9 个时钟读取低有效 ACK
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool aip650e_WriteByte(uint8_t value)
{
    uint8_t bit;
    bool acknowledged;

    /* 高位先发送 8bit 数据。 */
    for (bit = 0U; bit < 8U; ++bit)
    {
        aip650e_SetData((value & 0x80U) != 0U);
        aip650e_Delay();

        aip650e_SetClock(true);
        aip650e_Delay();

        aip650e_SetClock(false);
        aip650e_Delay();

        value <<= 1U;
    }

    /* 释放 DIO，在第 9 个时钟读取芯片 ACK。 */
    aip650e_SetData(true);
    aip650e_Delay();

    aip650e_SetClock(true);
    aip650e_Delay();

    acknowledged = (LL_GPIO_IsInputPinSet(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN) == 0U);

    aip650e_SetClock(false);
    aip650e_Delay();

    return acknowledged;
}


/*******************************************************************************
//  Function   : aip650e_WriteRegister
//  Description: 向 AiP650E 指定地址写入一个字节
//  Param      : address - 寄存器地址或命令
//               value   - 写入数据
//  Return     : true：写入成功；false：通信未应答
//  Others     : 一次通信依次发送地址和数据，两个字节均需要 ACK
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool aip650e_WriteRegister(uint8_t address, uint8_t value)
{
    bool acknowledged;

    aip650e_Start();

    acknowledged = aip650e_WriteByte(address);

    if (acknowledged)
    {
        acknowledged = aip650e_WriteByte(value);
    }

    aip650e_Stop();

    return acknowledged;
}


/*******************************************************************************
//  Function   : aip650e_WriteControl
//  Description: 写入 AiP650E 显示及亮度控制参数
//  Param      : brightness - 亮度等级
//               enabled    - true：开启显示；false：关闭显示
//  Return     : true：写入成功；false：通信未应答
//  Others     : 亮度等级 8 对应控制字段编码 0
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
static bool aip650e_WriteControl(uint8_t brightness, bool enabled)
{
    uint8_t brightnessBits = (brightness == AIP650E_BRIGHTNESS_MAX) ? 0U : brightness;
    uint8_t control = (uint8_t)(brightnessBits << AIP650E_BRIGHTNESS_SHIFT);

    if (enabled)
    {
        control |= AIP650E_CONTROL_DISPLAY_ON;
    }

    return aip650e_WriteRegister(AIP650E_CMD_SYSTEM, control);
}


/* Exported functions ------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650e_Init
//  Description: 初始化 AiP650E 通信引脚及显示 RAM
//  Param      : None
//  Return     : true：初始化成功；false：总线异常或芯片未应答
//  Others     : 先写入四个 COM 的空白数据，再按默认亮度开启显示
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_Init(void)
{
    uint8_t com;

    s_initialized = false;
    s_displayEnabled = false;
    s_brightness = AIP650E_DEFAULT_BRIGHTNESS;

    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOA);
    LL_IOP_GRP1_EnableClock(LL_IOP_GRP1_PERIPH_GPIOB);

    /* 先释放输出锁存，再将 CLK/DIO 切换为开漏；高电平由板上电阻提供。 */
    aip650e_SetClock(true);
    aip650e_SetData(true);
    LL_GPIO_SetPinOutputType(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinOutputType(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinPull(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinSpeed(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinSpeed(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinMode(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_MODE_OUTPUT);
    aip650e_Delay();

    if ((LL_GPIO_IsInputPinSet(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN) == 0U) ||
        (LL_GPIO_IsInputPinSet(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN) == 0U))
    {
        return false;
    }

    for (com = 0U; com < AIP650E_COM_COUNT; ++com)
    {
        if (!aip650e_WriteRegister((uint8_t)(AIP650E_ADDR_DIG1 + com * AIP650E_ADDR_STEP), 0U))
        {
            return false;
        }
    }

    if (!aip650e_WriteControl(s_brightness, true))
    {
        return false;
    }

    s_displayEnabled = true;
    s_initialized = true;
    return true;
}


/*******************************************************************************
//  Function   : aip650e_SetBrightness
//  Description: 设置 AiP650E 全局亮度
//  Param      : level - 亮度等级 1~8
//  Return     : true：写入成功；false：参数无效、未初始化或通信失败
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_SetBrightness(uint8_t level)
{
    if ((!s_initialized) || (level < AIP650E_BRIGHTNESS_MIN) || (level > AIP650E_BRIGHTNESS_MAX))
    {
        return false;
    }

    if (!aip650e_WriteControl(level, s_displayEnabled))
    {
        return false;
    }

    s_brightness = level;
    return true;
}


/*******************************************************************************
//  Function   : aip650e_SetDisplay
//  Description: 开关显示，保留显示 RAM 和亮度
//  Param      : enabled - true：开启显示；false：关闭显示
//  Return     : true：写入成功；false：未初始化或通信失败
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_SetDisplay(bool enabled)
{
    if ((!s_initialized) || (!aip650e_WriteControl(s_brightness, enabled)))
    {
        return false;
    }

    s_displayEnabled = enabled;
    return true;
}


/*******************************************************************************
//  Function   : aip650e_ShowSegments
//  Description: 写入 COM1 或 COM2 的七段段码
//  Param      : digit - 数码管位号 1~2；segments - A~G 对应 bit0~bit6
//  Return     : true：写入成功；false：参数无效、未初始化或通信失败
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowSegments(uint8_t digit, uint8_t segments)
{
    uint8_t address;

    if ((!s_initialized) || (digit == 0U) || (digit > AIP650E_DIGIT_COUNT) ||
        ((segments & AIP650E_SEG_DP) != 0U))
    {
        return false;
    }

    address = (uint8_t)(AIP650E_ADDR_DIG1 + (digit - 1U) * AIP650E_ADDR_STEP);
    return aip650e_WriteRegister(address, segments);
}


/*******************************************************************************
//  Function   : aip650e_ShowDigit
//  Description: 在 COM1 或 COM2 显示一位十进制数字
//  Param      : digit - 数码管位号 1~2；value - 0~9
//  Return     : true：写入成功；false：参数无效、未初始化或通信失败
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowDigit(uint8_t digit, uint8_t value)
{
    if (value > 9U)
    {
        return false;
    }

    return aip650e_ShowSegments(digit, s_numberSegments[value]);
}


/*******************************************************************************
//  Function   : aip650e_ShowNumber
//  Description: 在 COM1/COM2 显示 0~99
//  Param      : value - 待显示数值；leadingZeros - 是否显示前导零
//  Return     : true：两位均写入成功；false：参数无效、未初始化或通信失败
//  Others     : 不改变 COM3/COM4；若第二位写入失败，第一位不回滚
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowNumber(uint16_t value, bool leadingZeros)
{
    uint8_t tens;

    if ((!s_initialized) || (value > 99U))
    {
        return false;
    }

    tens = ((value >= 10U) || leadingZeros) ? s_numberSegments[value / 10U] : 0U;
    if (!aip650e_ShowSegments(1U, tens))
    {
        return false;
    }

    return aip650e_ShowSegments(2U, s_numberSegments[value % 10U]);
}


/*******************************************************************************
//  Function   : aip650e_ShowData
//  Description: 按显示板 COM1/COM2/COM3/COM4 连接关系写入四字节数据
//  Param      : data - 4 字节数组；COM3 的 D/E/F 不推定颜色，COM4 为 0/1 指示灯
//  Return     : true：四个 COM 均写入成功；false：参数无效、未初始化或通信失败
//  Others     : COM4 的 A/B 两个发光支路由一个逻辑开关同时控制
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_ShowData(const uint8_t *data)
{
    uint8_t com;
    uint8_t segments;

    if ((!s_initialized) || (data == NULL) ||
        ((data[AIP650E_COM1_INDEX] & AIP650E_SEG_DP) != 0U) ||
        ((data[AIP650E_COM2_INDEX] & AIP650E_SEG_DP) != 0U) ||
        ((data[AIP650E_COM3_INDEX] & (uint8_t)~AIP650E_COM3_VALID_MASK) != 0U) ||
        (data[AIP650E_COM4_INDEX] > AIP650E_COM4_ON))
    {
        return false;
    }

    for (com = 0U; com < AIP650E_COM_COUNT; ++com)
    {
        segments = data[com];
        if (com == AIP650E_COM4_INDEX)
        {
            segments = (data[com] == AIP650E_COM4_ON) ? (AIP650E_SEG_A | AIP650E_SEG_B) : 0U;
        }

        if (!aip650e_WriteRegister((uint8_t)(AIP650E_ADDR_DIG1 + com * AIP650E_ADDR_STEP), segments))
        {
            return false;
        }
    }

    return true;
}


/*******************************************************************************
//  Function   : aip650e_Clear
//  Description: 清空四个 COM 的显示 RAM
//  Param      : None
//  Return     : true：四个 COM 均清空；false：未初始化或通信失败
//  Author     : Chen Liang 2026-10-15
*******************************************************************************/
bool aip650e_Clear(void)
{
    uint8_t data[AIP650E_COM_COUNT] = {0U, 0U, 0U, 0U};

    return aip650e_ShowData(data);
}
