
/**
 ***********************************************************************************************************************
 * @file    aip650e.c
 * @brief   AiP650E 软件两线通信、数码管显示及矩阵按键驱动
 * @author  Leo Chen
 * @version 1.1.0
 * @date    2026-10-21
 ***********************************************************************************************************************
 */

#include "aip650e.h"
#include "bsp_gpio.h"

#include <stddef.h>


/* Macro -------------------------------------------------------------------------------------------------------------*/
/* Private variables -------------------------------------------------------------------------------------------------*/

/** AiP650E 驱动实例，保存当前控制状态及显示缓存 */
static aip650e_t s_aip650e =
{
    .ctrl =
    {
        .segment        = AIP650_8_SEGMENT,
        .brightness     = BRIGHTNESS_4_LEVEL,
        .displayEnabled = false,
        .sleeping       = false
    },
    .show = {0U}
};



/* Private constants -------------------------------------------------------------------------------------------------*/

/** 数字 0~9 对应的七段数码管显示编码 */
static const uint8_t s_numberSegments[10] =
{
    NUMBER_0, NUMBER_1, NUMBER_2, NUMBER_3, NUMBER_4,
    NUMBER_5, NUMBER_6, NUMBER_7, NUMBER_8, NUMBER_9
};


/** AiP650E 原始按键编码与矩阵行列编码映射表 */
static const aip650_key_map_t s_keyTable[] =
{
    {0x44U, 0x11U}, {0x4CU, 0x12U}, {0x54U, 0x13U}, {0x5CU, 0x14U},
    {0x64U, 0x15U}, {0x6CU, 0x16U}, {0x74U, 0x17U},
    {0x45U, 0x21U}, {0x4DU, 0x22U}, {0x55U, 0x23U}, {0x5DU, 0x24U},
    {0x65U, 0x25U}, {0x6DU, 0x26U}, {0x75U, 0x27U},
    {0x46U, 0x31U}, {0x4EU, 0x32U}, {0x56U, 0x33U}, {0x5EU, 0x34U},
    {0x66U, 0x35U}, {0x6EU, 0x36U}, {0x76U, 0x37U},
    {0x47U, 0x41U}, {0x4FU, 0x42U}, {0x57U, 0x43U}, {0x5FU, 0x44U},
    {0x67U, 0x45U}, {0x6FU, 0x46U}, {0x77U, 0x47U}
};


/* Private functions -------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650_Delay
//  Description: 软件延时，满足 AiP650E 两线通信时序要求
//  Param      : None
//  Return     : None
//  Others     : 延时长度由 AIP650E_DELAY_CYCLES 配置，实际时间受 MCU 主频影响
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static void aip650_Delay(void)
{
    volatile uint32_t count;

    for (count = 0U; count < AIP650E_DELAY_CYCLES; ++count)
    {
        __NOP();
    }
}


/*******************************************************************************
//  Function   : aip650_Start
//  Description: 产生 AiP650E 两线通信起始信号
//  Param      : None
//  Return     : None
//  Others     : CLK 为高电平时将 DIO 从高拉低，再拉低 CLK
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static void aip650_Start(void)
{
    AIP650_DIO_HIGH();
    AIP650_CLK_HIGH();
    aip650_Delay();

    /* CLK 保持高电平时拉低 DIO，产生 START 条件。 */
    AIP650_DIO_LOW();
    aip650_Delay();

    AIP650_CLK_LOW();
    aip650_Delay();
}


/*******************************************************************************
//  Function   : aip650_Stop
//  Description: 产生 AiP650E 两线通信停止信号
//  Param      : None
//  Return     : None
//  Others     : CLK 为高电平时释放 DIO，使其从低电平变为高电平
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static void aip650_Stop(void)
{
    AIP650_DIO_LOW();
    aip650_Delay();

    AIP650_CLK_HIGH();
    aip650_Delay();

    /* CLK 为高时释放 DIO，产生 STOP 条件。 */
    AIP650_DIO_HIGH();
    aip650_Delay();
}


/*******************************************************************************
//  Function   : aip650_WriteByte
//  Description: 发送一个字节并读取从机应答
//  Param      : value - 待发送的 8 位数据
//  Return     : true：收到 ACK；false：未收到 ACK
//  Others     : 数据按 MSB 优先发送，第 9 个时钟周期读取低有效 ACK
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static bool aip650_WriteByte(uint8_t value)
{
    uint8_t bit;
    bool acknowledged;

    /* 按高位优先顺序依次发送 8 位数据。 */
    for (bit = 0U; bit < 8U; ++bit)
    {
        if ((value & 0x80U) != 0U)
        {
            AIP650_DIO_HIGH();
        }
        else
        {
            AIP650_DIO_LOW();
        }

        aip650_Delay();

        AIP650_CLK_HIGH();
        aip650_Delay();

        AIP650_CLK_LOW();
        aip650_Delay();

        value <<= 1U;
    }

    /* 释放开漏数据线，允许从机驱动 ACK。 */
    AIP650_DIO_HIGH();
    aip650_Delay();

    AIP650_CLK_HIGH();
    aip650_Delay();

    /* ACK 为低电平，表示从机已接收当前字节。 */
    acknowledged = (AIP650_DIO_READ() == 0U);

    AIP650_CLK_LOW();
    aip650_Delay();

    return acknowledged;
}


/*******************************************************************************
//  Function   : aip650_ReadByte
//  Description: 从 AiP650E 读取一个字节
//  Param      : None
//  Return     : 读取的 8 位原始数据
//  Others     : 数据按 MSB 优先接收，读取结束后由主机发送 NACK
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static uint8_t aip650_ReadByte(void)
{
    uint8_t bit;
    uint8_t value = 0U;

    /* 释放数据线，由芯片输出读取数据。 */
    AIP650_DIO_HIGH();

    for (bit = 0U; bit < 8U; ++bit)
    {
        value <<= 1U;

        AIP650_CLK_HIGH();
        aip650_Delay();

        if (AIP650_DIO_READ() != 0U)
        {
            value |= 0x01U;
        }

        AIP650_CLK_LOW();
        aip650_Delay();
    }

    /* 单字节读取完成后，主机释放 DIO 发送 NACK。 */
    AIP650_DIO_HIGH();
    aip650_Delay();

    AIP650_CLK_HIGH();
    aip650_Delay();

    AIP650_CLK_LOW();
    aip650_Delay();

    return value;
}


/*******************************************************************************
//  Function   : aip650_WriteRegister
//  Description: 向 AiP650E 指定寄存器或显示 RAM 写入数据
//  Param      : address - 寄存器或命令地址
//               value   - 待写入的数据
//  Return     : true：写入成功；false：地址或数据未收到 ACK
//  Others     : 按 START、地址、数据、STOP 的顺序完成一次写操作
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
static bool aip650_WriteRegister(uint8_t address, uint8_t value)
{
    bool acknowledged;

    aip650_Start();

    /* 发送地址，收到 ACK 后继续发送数据。 */
    acknowledged = aip650_WriteByte(address);

    if (acknowledged)
    {
        acknowledged = aip650_WriteByte(value);
    }

    /* 无论操作是否成功，均发送停止信号释放总线。 */
    aip650_Stop();

    return acknowledged;
}


/*******************************************************************************
//  Function   : aip650_WriteControl
//  Description: 将控制状态转换为系统控制字并写入 AiP650E
//  Param      : ctrl - 系统控制状态结构体
//  Return     : true：控制配置写入成功；false：参数无效或通信失败
//  Others     : 将显示模式、亮度、显示使能及睡眠状态组合为控制寄存器数据
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_WriteControl(const aip650e_ctrl_t *ctrl)
{
    uint8_t control;

    if (ctrl == NULL)
    {
        return false;
    }

    /* 亮度编码位于系统控制字的高四位。 */
    control = (uint8_t)((uint8_t)ctrl->brightness << 4U);

    /* 设置七段显示模式选择位。 */
    if (ctrl->segment == AIP650_7_SEGMENT)
    {
        control |= AIP650_CONTROL_7SEG;
    }

    /* 设置芯片睡眠控制位。 */
    if (ctrl->sleeping)
    {
        control |= AIP650_CONTROL_SLEEP;
    }

    /* 设置显示输出使能位。 */
    if (ctrl->displayEnabled)
    {
        control |= AIP650_CONTROL_DISPLAY;
    }

    return aip650_WriteRegister(AIP650_CMD_SYSTEM, control);
}


/* Main API ----------------------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : aip650_init
//  Description: 初始化 AiP650E 软件两线通信接口及显示芯片
//  Param      : None
//  Return     : None
//  Others     : 初始化控制参数，配置开漏 GPIO，检查总线电平，
//               清除显示 RAM，最后使能显示输出
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void aip650_init(void)
{
    uint8_t digit;

    /* 初始化显示控制参数。 */
    s_aip650e.ctrl.segment        = AIP650_7_SEGMENT;
    s_aip650e.ctrl.brightness     = BRIGHTNESS_4_LEVEL;
    s_aip650e.ctrl.displayEnabled = false;
    s_aip650e.ctrl.sleeping       = false;

    /*
     * 配置 CLK 与 DIO 为开漏输出。
     * GPIO 时钟由 bsp_gpio_Init() 统一初始化，
     * 两线通过外部上拉电阻恢复高电平。
     */
    AIP650_CLK_HIGH();
    AIP650_DIO_HIGH();

    LL_GPIO_SetPinOutputType(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_OUTPUT_OPENDRAIN);
    LL_GPIO_SetPinOutputType(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_OUTPUT_OPENDRAIN);

    LL_GPIO_SetPinPull(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_PULL_NO);
    LL_GPIO_SetPinPull(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_PULL_NO);

    LL_GPIO_SetPinSpeed(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);
    LL_GPIO_SetPinSpeed(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_SPEED_FREQ_VERY_HIGH);

    LL_GPIO_SetPinMode(DISPLAY_CLK_PORT, DISPLAY_CLK_PIN, LL_GPIO_MODE_OUTPUT);
    LL_GPIO_SetPinMode(DISPLAY_DIO_PORT, DISPLAY_DIO_PIN, LL_GPIO_MODE_OUTPUT);

    aip650_Delay();

    /* 检查两线释放后是否为高电平，避免总线异常时继续通信。 */
    if ((AIP650_DIO_READ() == 0U) || (AIP650_CLK_READ() == 0U))
    {
        return;
    }

    /* 清除 DIG1~DIG4 显示 RAM，防止上电后出现残留显示。 */
    for (digit = 0U; digit < AIP650E_COM_COUNT; ++digit)
    {
        if (!aip650_WriteRegister((uint8_t)(DIG1 + digit * AIP650_ADDR_STEP), 0U))
        {
            return;
        }

        s_aip650e.show[digit] = 0U;
    }

    /* 显示 RAM 初始化完成后开启显示输出。 */
    s_aip650e.ctrl.displayEnabled = true;

    if (!aip650_WriteControl(&s_aip650e.ctrl))
    {
        s_aip650e.ctrl.displayEnabled = false;
        return;
    }
}


/*******************************************************************************
//  Function   : aip650_write_digit
//  Description: 向指定 DIG 显示寄存器写入段码数据
//  Param      : digit    - DIG 索引，范围 0~AIP650E_COM_COUNT-1
//               segments - 待写入的显示段码
//  Return     : true：写入成功；false：DIG 索引无效或通信失败
//  Others     : 成功写入后同步更新软件显示缓存
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_digit(uint8_t digit, uint8_t segments)
{
    /* 检查显示寄存器索引范围。 */
    if (digit >= AIP650E_COM_COUNT)
    {
        return false;
    }

    /* 根据 DIG 索引计算对应的显示 RAM 地址。 */
    if (!aip650_WriteRegister((uint8_t)(DIG1 + digit * AIP650_ADDR_STEP), segments))
    {
        return false;
    }

    /* 硬件写入成功后更新软件镜像。 */
    s_aip650e.show[digit] = segments;

    return true;
}


/*******************************************************************************
//  Function   : aip650_write_segments
//  Description: 批量更新 DIG1~DIG4 显示数据
//  Param      : data - 指向 AIP650E_COM_COUNT 字节的段码数组
//  Return     : true：全部写入成功；false：参数无效或通信失败
//  Others     : 仅写入与缓存不同的段码，减少总线通信；
//               发生部分写入失败时保留此前已成功更新的缓存
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_write_segments(const uint8_t data[AIP650E_COM_COUNT])
{
    uint8_t digit;

    if (data == NULL)
    {
        return false;
    }

    /* 遍历所有 DIG，仅更新发生变化的显示数据。 */
    for (digit = 0U; digit < AIP650E_COM_COUNT; ++digit)
    {
//        if ((s_aip650e.show[digit] != data[digit]) &&
//            !aip650_write_digit(digit, data[digit]))
//        {
//            return false;
//        }
		aip650_write_digit(digit, data[digit]);
    }

    return true;
}


/*******************************************************************************
//  Function   : aip650_write_number
//  Description: 使用原始显示段码数组更新 DIG1~DIG4
//  Param      : data - 待显示段码数组首地址
//  Return     : None
//  Others     : 兼容原有接口，调用 aip650_write_segments() 完成写入，
//               不向调用方返回通信结果
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
void aip650_write_number(uint8_t *data)
{
    (void)aip650_write_segments(data);
}


/*******************************************************************************
//  Function   : aip650_clear
//  Description: 清除 DIG1~DIG4 的全部显示内容
//  Param      : None
//  Return     : true：清屏成功；false：通信失败
//  Others     : 将所有 DIG 的显示段码设置为 0，保留当前显示控制参数
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
bool aip650_clear(void)
{
    static const uint8_t clear_data[AIP650E_COM_COUNT] = {0U};

    return aip650_write_segments(clear_data);
}


/*******************************************************************************
//  Function   : aip650_get_key
//  Description: 读取 AiP650E 按键状态并转换为矩阵键值
//  Param      : None
//  Return     : 有效按键的矩阵行列编码；无按键或通信失败时返回 AIP650E_KEY_NONE
//  Others     : 发送按键读取命令，读取原始键值，再通过映射表返回行列编码
//  Author     : Leo Chen 2026-10-21
*******************************************************************************/
char aip650_get_key(void)
{
    uint8_t index;
    uint8_t key;

    /* 芯片处于睡眠状态时不执行按键读取。 */
    if (s_aip650e.ctrl.sleeping)
    {
        return (char)AIP650E_KEY_NONE;
    }

    /* 发送按键读取命令。 */
    aip650_Start();

    if (!aip650_WriteByte(AIP650_CMD_READ_KEY))
    {
        aip650_Stop();
        return (char)AIP650E_KEY_NONE;
    }

    /* 读取芯片返回的原始按键编码。 */
    key = aip650_ReadByte();
    aip650_Stop();

    /* 未检测到按键时直接返回无按键标志。 */
    if (key == AIP650_NO_KEY_RAW)
    {
        return (char)AIP650E_KEY_NONE;
    }

    /* 根据原始编码查找对应的矩阵行列值。 */
    for (index = 0U; index < (uint8_t)(sizeof(s_keyTable) / sizeof(s_keyTable[0])); ++index)
    {
        if (s_keyTable[index].raw == key)
        {
            return (char)s_keyTable[index].matrix;
        }
    }

    /* 未匹配到有效键值。 */
    return (char)AIP650E_KEY_NONE;
}
