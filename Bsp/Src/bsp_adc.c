/**
 ***********************************************************************************************************************
 * @file    bsp_adc.c
 * @brief   ADC 底层驱动，实现 ADC 多通道扫描、中断/DMA 数据采集及启停控制
 * @author  Leo Chen
 * @version 1.0.0
 * @date    2026-08-18
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

/* Includes ----------------------------------------------------------------------------------------------------------*/
#include "bsp_adc.h"


/* Private define ----------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC 操作超时时间
 *
 * 用于 ADC 校准、ADC 使能以及 ADC 停止转换等操作的超时保护，
 * 防止硬件状态异常导致程序永久阻塞。
 */
#define BSP_ADC_OPERATION_TIMEOUT             (100000UL)


/* Variable ----------------------------------------------------------------------------------------------------------*/

/**
 * @brief ADC 多通道转换结果缓存
 *
 * 数组下标与 adc_Channel_t 枚举一一对应：
 *
 * ADCxConvertedDatas[ADC_CH_MOTOR] -> MOTOR 电流反馈
 * ADCxConvertedDatas[ADC_CH_WH]    -> WH
 * ADCxConvertedDatas[ADC_CH_SW]    -> SW
 * ADCxConvertedDatas[ADC_CH_USB]   -> USB 电压
 * ADCxConvertedDatas[ADC_CH_NTC]   -> NTC 温度
 * ADCxConvertedDatas[ADC_CH_PGA2]  -> PGA2
 */
__IO uint32_t ADCxConvertedDatas[ADC_CH_NUM];

/**
 * @brief 当前正在处理的 ADC 逻辑通道索引
 *
 * 非 DMA 模式下，每产生一次 EOC 中断递增一次，
 * 用于将当前 ADC Rank 转换结果保存到对应缓存位置。
 */
__IO adc_Channel_t s_adcConvChannel = (adc_Channel_t)0;


/*******************************************************************************
//  Function   : bsp_adc_ChannelConvCpltCallback
//  Description: ADC 一组通道转换完成回调函数
//  Param      : Value - ADC 转换结果缓存首地址
//               Size  - ADC 转换数据数量
//  Return     : None
//  Others     : Weak 函数，应用层可重新实现该函数以处理 ADC 数据
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_adc_ChannelConvCpltCallback(volatile uint32_t *Value, uint16_t Size)
{
    (void)Value;
    (void)Size;
}

/*******************************************************************************
//  Function   : bsp_adc_ErrorCallback
//  Description: ADC 错误处理回调函数
//  Param      : None
//  Return     : None
//  Others     : Weak 函数，应用层可重新实现该函数进行错误记录或异常处理
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
__weak void bsp_adc_ErrorCallback(void){}


/*******************************************************************************
//  Function   : bsp_adc_io_Init
//  Description: 初始化 ADC GPIO 及 ADC 中断
//  Param      : None
//  Return     : None
//  Others     : 将外部 ADC 输入引脚配置为模拟模式；非 DMA 模式下开启 ADC IRQ
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_adc_io_config(void)
{
    /* -------------------------------------------------------------------------------------------------------------- */
    /* MOTOR 电流反馈 ADC                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */
    BSP_ADC_MOTOR_GPIO_CLOCK_EN();
    LL_GPIO_SetPinMode(BSP_ADC_MOTOR_GPIO_PORT,BSP_ADC_MOTOR_GPIO_PIN,LL_GPIO_MODE_ANALOG);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* WH ADC                                                                                                         */
    /* -------------------------------------------------------------------------------------------------------------- */
    BSP_ADC_WH_GPIO_CLOCK_EN();
    LL_GPIO_SetPinMode(BSP_ADC_WH_GPIO_PORT,BSP_ADC_WH_GPIO_PIN,LL_GPIO_MODE_ANALOG);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* SW ADC                                                                                                         */
    /* -------------------------------------------------------------------------------------------------------------- */
    BSP_ADC_SW_GPIO_CLOCK_EN();
    LL_GPIO_SetPinMode(BSP_ADC_SW_GPIO_PORT,BSP_ADC_SW_GPIO_PIN,LL_GPIO_MODE_ANALOG);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* USB 输入电压检测 ADC                                                                                           */
    /* -------------------------------------------------------------------------------------------------------------- */
    BSP_ADC_USB_GPIO_CLOCK_EN();
    LL_GPIO_SetPinMode(BSP_ADC_USB_GPIO_PORT,BSP_ADC_USB_GPIO_PIN,LL_GPIO_MODE_ANALOG);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* NTC 温度检测 ADC                                                                                               */
    /* -------------------------------------------------------------------------------------------------------------- */
    BSP_ADC_NTC_GPIO_CLOCK_EN();
    LL_GPIO_SetPinMode(BSP_ADC_NTC_GPIO_PORT,BSP_ADC_NTC_GPIO_PIN,LL_GPIO_MODE_ANALOG);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 中断配置                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

#if (ADC_USE_DMA == 0)
    /*
     * 非 DMA 模式下，每个 Rank 转换完成后通过 EOC 中断读取 ADC 数据。
     */
    NVIC_SetPriority(ADC_IRQn, 1);
    NVIC_EnableIRQ(ADC_IRQn);

#endif
}


/*******************************************************************************
//  Function   : bsp_adc_rank_Init
//  Description: 配置 ADC 规则组扫描序列及各通道采样时间
//  Param      : None
//  Return     : None
//  Others     : ADC 共配置 6 个 Rank，Rank 顺序需与 adc_Channel_t 枚举顺序保持一致
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_adc_rank_config(void)
{
    /*
     * ADC 规则组采用 7 Rank 扫描模式。
     *
     * Rank1 -> MOTOR
     * Rank2 -> WH
     * Rank3 -> SW
     * Rank4 -> USB
     * Rank5 -> NTC
     * Rank6 -> PGA2
     * Rank7 -> VREFINT
     *
     * 注意：
     * 此处 Rank 顺序必须与 adc_Channel_t 枚举顺序保持一致，
     * 否则 ADCxConvertedDatas[] 中的数据与实际信号会发生错位。
     */
    LL_ADC_REG_SetSequencerLength(ADC1,LL_ADC_REG_SEQ_SCAN_ENABLE_7RANKS);

    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_1,BSP_ADC_MOTOR_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_2,BSP_ADC_WH_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_3,BSP_ADC_SW_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_4,BSP_ADC_USB_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_5,BSP_ADC_NTC_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_6,BSP_ADC_PGA2_CHANNEL);
    LL_ADC_REG_SetSequencerRanks(ADC1,LL_ADC_REG_RANK_7,BSP_ADC_VREFINT_CHANNEL);

    /*
     * 所有 ADC 通道统一使用 640.5 个 ADC 时钟周期采样时间。
     *
     * 较长的采样时间能够给 ADC 采样保持电容提供更充分的充电时间，
     * 对高阻抗信号源、NTC 分压等模拟信号更加稳定。
     */
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_MOTOR_CHANNEL,    LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_WH_CHANNEL,       LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_SW_CHANNEL,       LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_USB_CHANNEL,      LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_NTC_CHANNEL,      LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_PGA2_CHANNEL,     LL_ADC_SAMPLINGTIME_640CYCLES_5);
    LL_ADC_SetChannelSamplingTime(ADC1, BSP_ADC_VREFINT_CHANNEL,  LL_ADC_SAMPLINGTIME_640CYCLES_5);
}


#if ADC_USE_DMA

/*******************************************************************************
//  Function   : bsp_adc_dma_Init
//  Description: 初始化 ADC DMA 数据传输
//  Param      : None
//  Return     : None
//  Others     : 使用 DMA1 Channel1 将 ADC1->DR 自动搬运至 ADCxConvertedDatas[]
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
static void bsp_adc_dma_config(void)
{
    LL_DMA_InitTypeDef DMA_InitStruct = {0};

    /* -------------------------------------------------------------------------------------------------------------- */
    /* DMA 时钟及映射配置                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);                   /* 使能 DMA1 外设时钟。 */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_SYSCFG);                 /* 使能 SYSCFG 时钟。 */
    LL_SYSCFG_SetDMARemap(DMA1, LL_DMA_CHANNEL_1, LL_SYSCFG_DMA_MAP_ADC); /* DMA1 Channel1 映射至 ADC DMA 请求。 */

    /* -------------------------------------------------------------------------------------------------------------- */
    /* DMA 数据传输参数                                                                                               */
    /* -------------------------------------------------------------------------------------------------------------- */
    DMA_InitStruct.PeriphOrM2MSrcAddress  = (uint32_t)&ADC1->DR;                /* DMA 源地址固定为 ADC1 数据寄存器 ADC1->DR。 */
    DMA_InitStruct.MemoryOrM2MDstAddress  = (uint32_t)&ADCxConvertedDatas;      /* DMA 目标地址为 ADC 多通道数据缓存。 */
    DMA_InitStruct.Direction              = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;  /* ADC -> RAM。 */
    DMA_InitStruct.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_FIX;                  /* ADC 数据寄存器地址固定不变。 */
    DMA_InitStruct.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;            /* RAM 地址自动递增， 每完成一个 Rank 自动写入下一个数组成员。*/
    DMA_InitStruct.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_WORD;             /* ADC 外设数据宽度：32 bit。 */
    DMA_InitStruct.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_WORD;             /* RAM 数据宽度：32 bit。 */
    DMA_InitStruct.Mode                   = LL_DMA_MODE_INFINITE_CIRCULAR;      /* 无限循环模式。 每完成 ADC_CH_NUM 个数据后重新从缓存首地址开始。 */
    DMA_InitStruct.Priority               = LL_DMA_PRIORITY_0;                  /* DMA 优先级。 */
    DMA_InitStruct.TransMode              = LL_DMA_TRANSFER_SINGLE;             /* 每次 DMA 请求传输一个数据。 */
    DMA_InitStruct.NbData                 = ADC_CH_NUM;                         /* 一次完整 ADC 扫描序列包含 ADC_CH_NUM 个转换结果。 当前 ADC_CH_NUM = 6。 */

    /* 初始化 DMA1 Channel1。 */
    LL_DMA_Init(DMA1,LL_DMA_CHANNEL_1, &DMA_InitStruct);
    /*
     * 开启 Block Transfer Complete 中断。
     * 当一组 ADC_CH_NUM 数据全部传输完成后触发 DMA 中断。
     */
    LL_DMA_EnableIT_BTC(DMA1, LL_DMA_CHANNEL_1);

    /* 配置并使能 DMA1 Channel1 中断。 */
    NVIC_SetPriority(DMA1_Channel1_IRQn, 0);
    NVIC_EnableIRQ(DMA1_Channel1_IRQn);

    /* 使能 DMA Channel。 */
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1); 
}

#endif /* ADC_USE_DMA */


/*******************************************************************************
//  Function   : bsp_adc_Init
//  Description: 初始化 ADC 模块
//  Param      : None
//  Return     : None
//  Others     : ADC 默认使用 12 bit、右对齐、TIM3 TRGO 外部触发、7 Rank 扫描模式
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Init(void)
{
    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 时钟                                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 时钟源选择 SYSCLK。 */
    LL_RCC_SetADCClockSource(LL_RCC_ADC_CLKSOURCE_SYSCLK);

    /* 使能 ADC1 外设时钟。 */
    LL_APB1_GRP2_EnableClock(LL_APB1_GRP2_PERIPH_ADC1);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC GPIO                                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */
    bsp_adc_io_config();

    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 基础配置                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_ADC_SetDataAlignment(ADC1, LL_ADC_DATA_ALIGN_RIGHT);    /* ADC 转换结果右对齐。 */
    LL_ADC_SetResolution(ADC1, LL_ADC_RESOLUTION_12B);         /* ADC 分辨率：12 bit。 */
    LL_ADC_SetLowPowerMode(ADC1, LL_ADC_LP_MODE_NONE);         /* 不启用 ADC 内部低功耗模式。 */

    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 触发配置                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 使用 TIM3 TRGO 作为规则组外部触发源。
     *
     * 每次 TIM1 产生 TRGO 后，
     * ADC 按 Rank1 -> Rank7 的顺序完成一次扫描。
     */
    LL_ADC_REG_SetTriggerSource(ADC1, BSP_ADC_TRIGGER_SOURCE);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC DMA 配置                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

#if ADC_USE_DMA
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_UNLIMITED);                 /* DMA 无限传输模式， ADC 每产生一次转换结果，通过 DMA 自动搬运至 RAM。 */
#else
    LL_ADC_REG_SetDMATransfer(ADC1, LL_ADC_REG_DMA_TRANSFER_NONE);                      /* 非 DMA 模式，软件在 ADC EOC 中断中读取转换结果。 */
#endif


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 转换模式                                                                                                    */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_ADC_REG_SetContinuousMode(ADC1, LL_ADC_REG_CONV_SINGLE);                         /* ADC单次扫描模 */
    LL_ADC_REG_SetOverrun(ADC1, LL_ADC_REG_OVR_DATA_OVERWRITTEN);                       /* ADC Overrun 时，新转换结果覆盖旧转换结果 */
    LL_ADC_SetCommonClock(__LL_ADC_COMMON_INSTANCE(ADC1), LL_ADC_CLOCK_SYNC_PCLK_DIV4); /* ADC 公共时钟：PCLK / 4。 */

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 使能 VREFINT 内部通道路径                                                                                      */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_ADC_SetCommonPathInternalChAdd(__LL_ADC_COMMON_INSTANCE(ADC1),LL_ADC_PATH_INTERNAL_VREFINT);

    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC Rank 配置                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */
    bsp_adc_rank_config();

    /* -------------------------------------------------------------------------------------------------------------- */
    /* DMA 初始化                                                                                                     */
    /* -------------------------------------------------------------------------------------------------------------- */
#if ADC_USE_DMA
    bsp_adc_dma_config();
#endif

    /* 初始化非 DMA 模式下的软件 Rank 索引。 */
    s_adcConvChannel = (adc_Channel_t)0;
}


/*******************************************************************************
//  Function   : bsp_adc_Deinit
//  Description: 反初始化 ADC 模块
//  Param      : None
//  Return     : None
//  Others     : 停止 ADC 转换，并释放 ADC IRQ / DMA 相关资源
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Deinit(void)
{
    /*
     * ADC 时钟处于开启状态时才执行 ADC 停止及反初始化，
     * 避免访问未提供外设时钟的 ADC 寄存器。
     */
    if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_ADC1) != 0U)
    {
        /* 停止 ADC 当前转换并关闭 ADC。 */
        bsp_adc_Stop();

        /*
         * ADC 外设恢复至复位状态。
         */
        if (LL_ADC_DeInit(ADC1) != SUCCESS)
        {
            bsp_adc_ErrorCallback();
        }

        /* 关闭 ADC1 外设时钟。 */
        LL_APB1_GRP2_DisableClock(LL_APB1_GRP2_PERIPH_ADC1);
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* IRQ / DMA 资源释放                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */

#if ADC_USE_DMA

    /* 关闭 DMA IRQ。 */
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    NVIC_ClearPendingIRQ(DMA1_Channel1_IRQn);

    /* 关闭 DMA Block Transfer Complete 中断。 */
    LL_DMA_DisableIT_BTC(DMA1, LL_DMA_CHANNEL_1);

    /* 禁止 DMA Channel。 */
    LL_DMA_DisableChannel(DMA1, LL_DMA_CHANNEL_1);

    /* DMA Channel 恢复默认状态。 */
    (void)LL_DMA_DeInit(DMA1, LL_DMA_CHANNEL_1);

#else

    /* 非 DMA 模式下关闭 ADC IRQ。 */
    NVIC_DisableIRQ(ADC_IRQn);
    NVIC_ClearPendingIRQ(ADC_IRQn);

#endif


    /* 复位软件 Rank 索引。 */
    s_adcConvChannel = (adc_Channel_t)0;
}


/*******************************************************************************
//  Function   : bsp_adc_Start
//  Description: 启动 ADC 采样
//  Param      : None
//  Return     : None
//  Others     : 完成 ADC 校准、使能 ADC、中断/DMA配置，并等待 TIM3 TRGO 触发转换
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Start(void)
{
    uint32_t timeout;


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 时钟检查                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * ADC 外设时钟未开启时无法访问 ADC，
     * 直接执行错误回调并退出。
     */
    if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_ADC1) == 0U)
    {
        bsp_adc_ErrorCallback();
        return;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 转换状态检查                                                                                               */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * ADC 已经处于转换状态时直接返回，
     * 防止重复启动 ADC 转换。
     */
    if (LL_ADC_REG_IsConversionOngoing(ADC1) != 0U)
    {
        return;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 校准及使能                                                                                                 */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 仅在 ADC 尚未使能时进行校准。
     *
     * bsp_adc_Stop() 会关闭 ADC，
     * 因此下一次 Start 时会重新完成 ADC 校准。
     */
    if (LL_ADC_IsEnabled(ADC1) == 0U)
    {
        /*
         * 复位 ADC 校准逻辑。
         */
        LL_ADC_ResetCalibration(ADC1);
        timeout = BSP_ADC_OPERATION_TIMEOUT;

        while ((LL_ADC_IsResetCalibrationOnGoing(ADC1) != 0U) && (timeout > 0U))
        {
            timeout--;
        }

        if (timeout == 0U)
        {
            bsp_adc_ErrorCallback();
            return;
        }


        /*
         * 启动 ADC 校准。
         *
         * 使用 8 次校准采样。
         */
        LL_ADC_StartCalibration(ADC1, LL_ADC_CALIBRATION_NUMBER_8);
		
        timeout = BSP_ADC_OPERATION_TIMEOUT;
        while ((LL_ADC_IsCalibrationOnGoing(ADC1) != 0U) && (timeout > 0U))
        {
            timeout--;
        }
        if (timeout == 0U)
        {
            bsp_adc_ErrorCallback();
            return;
        }


        /*
         * 清除 ADC Ready 标志，
         * 为后续 ADC 使能做准备。
         */
        LL_ADC_ClearFlag_ADRDY(ADC1);


        /*
         * 使能 ADC。
         *
         * 部分情况下 LL_ADC_Enable() 后需要等待硬件真正进入 Enable 状态，
         * 因此加入超时保护。
         */
        timeout = BSP_ADC_OPERATION_TIMEOUT;
        do
        {
            LL_ADC_Enable(ADC1);
            timeout--;
        } while ((LL_ADC_IsEnabled(ADC1) == 0U) && (timeout > 0U));


        /*
         * 等待 ADC Ready。
         */
        timeout = BSP_ADC_OPERATION_TIMEOUT;
        while ((LL_ADC_IsActiveFlag_ADRDY(ADC1) == 0U) && (timeout > 0U))
        {
            timeout--;
        }

        if (timeout == 0U)
        {
            bsp_adc_ErrorCallback();
            return;
        }
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 清除 ADC 状态标志                                                                                              */
    /* -------------------------------------------------------------------------------------------------------------- */

    LL_ADC_ClearFlag_EOC(ADC1);
    LL_ADC_ClearFlag_EOS(ADC1);
    LL_ADC_ClearFlag_OVR(ADC1);

    /* 新的一轮转换从 MOTOR / Rank1 开始。 */
    s_adcConvChannel = (adc_Channel_t)0;


    /* -------------------------------------------------------------------------------------------------------------- */
    /* DMA / ADC 中断使能                                                                                             */
    /* -------------------------------------------------------------------------------------------------------------- */

#if ADC_USE_DMA

    /*
     * 清除 DMA 历史状态，
     * 防止上一次 DMA 传输留下的标志立即触发中断。
     */
    LL_DMA_ClearFlag_TC1(DMA1);
    LL_DMA_ClearFlag_BTC1(DMA1);
    LL_DMA_ClearFlag_HBT1(DMA1);
    LL_DMA_ClearFlag_TE1(DMA1);

    /* 重新开启 DMA Channel。 */
    LL_DMA_EnableChannel(DMA1, LL_DMA_CHANNEL_1);

#else

    /*
     * 非 DMA 模式：
     *
     * EOC：每个 Rank 转换完成时产生中断
     * OVR：ADC 数据发生覆盖时产生中断
     */
    LL_ADC_EnableIT_EOC(ADC1);
    LL_ADC_EnableIT_OVR(ADC1);

#endif


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 启动 ADC                                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * 使 ADC 规则组进入等待外部触发状态。
     *
     * 真正的转换由 TIM3 TRGO 启动。
     */
    LL_ADC_REG_StartConversion(ADC1);
}


/*******************************************************************************
//  Function   : bsp_adc_Stop
//  Description: 停止 ADC 采样
//  Param      : None
//  Return     : None
//  Others     : 停止 ADC 转换并关闭 ADC、中断及 DMA 数据传输
//  Author     : Chen Liang 2026-08-18
*******************************************************************************/
void bsp_adc_Stop(void)
{
    uint32_t timeout = BSP_ADC_OPERATION_TIMEOUT;
    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 时钟检查                                                                                                   */
    /* -------------------------------------------------------------------------------------------------------------- */

    /*
     * ADC 时钟已经关闭时无需继续访问 ADC 寄存器。
     */
    if (LL_APB1_GRP2_IsEnabledClock(LL_APB1_GRP2_PERIPH_ADC1) == 0U)
    {
        s_adcConvChannel = (adc_Channel_t)0;
        return;
    }

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 关闭 ADC 中断                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */
    LL_ADC_DisableIT_EOC(ADC1);
    LL_ADC_DisableIT_OVR(ADC1);


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 停止 ADC 转换                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */
    /*
     * ADC 已使能且当前正在转换，同时尚未执行停止操作时，
     * 请求 ADC 停止规则组转换。
     */
    if ((LL_ADC_IsEnabled(ADC1) != 0U) && (LL_ADC_REG_IsConversionOngoing(ADC1) != 0U) && (LL_ADC_REG_IsStopConversionOngoing(ADC1) == 0U))
    {
        LL_ADC_REG_StopConversion(ADC1);
    }


    /*
     * 等待 ADC 停止转换完成。
     */
    while ((LL_ADC_REG_IsStopConversionOngoing(ADC1) != 0U) && (timeout > 0U))
    {
        timeout--;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* DMA 停止                                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */

#if ADC_USE_DMA
    LL_DMA_DisableChannel(DMA1,LL_DMA_CHANNEL_1);
#endif


    /* -------------------------------------------------------------------------------------------------------------- */
    /* 关闭 ADC                                                                                                       */
    /* -------------------------------------------------------------------------------------------------------------- */
    if ((timeout > 0U) && (LL_ADC_IsEnabled(ADC1) != 0U))
    {
        LL_ADC_Disable(ADC1);
        while ((LL_ADC_IsEnabled(ADC1) != 0U) && (timeout > 0U))
        {
            timeout--;
        }
    }

    /* -------------------------------------------------------------------------------------------------------------- */
    /* 清除 ADC 标志                                                                                                  */
    /* -------------------------------------------------------------------------------------------------------------- */

    LL_ADC_ClearFlag_ADRDY(ADC1);
    LL_ADC_ClearFlag_EOC(ADC1);
    LL_ADC_ClearFlag_EOS(ADC1);
    LL_ADC_ClearFlag_OVR(ADC1);

    /* 软件 Rank 索引重新从 Rank1 开始。 */
    s_adcConvChannel = (adc_Channel_t)0;

    /*
     * ADC 停止或关闭超时。
     */
    if (timeout == 0U)
    {
        bsp_adc_ErrorCallback();
    }
}


/*******************************************************************************
//  Function   : ADC_IRQHandler
//  Description: ADC 中断服务函数
//  Param      : None
//  Return     : None
//  Others     : 非 DMA 模式下依次读取各 Rank 转换结果，并在完整序列结束后调用完成回调
//  Author     : Leo Chen 2026-08-18
*******************************************************************************/
void ADC_IRQHandler(void)
{
    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC Overrun 异常                                                                                               */
    /* -------------------------------------------------------------------------------------------------------------- */

    if ((LL_ADC_IsActiveFlag_OVR(ADC1) != RESET) && (LL_ADC_IsEnabledIT_OVR(ADC1) != RESET))
    {
        /* 清除 Overrun 标志。 */
        LL_ADC_ClearFlag_OVR(ADC1);

        /* Rank 软件索引重新同步至 Rank1。 */
        s_adcConvChannel = (adc_Channel_t)0U;

        /* 通知应用层 ADC 发生异常。 */
        bsp_adc_ErrorCallback();

        return;
    }


    /* -------------------------------------------------------------------------------------------------------------- */
    /* ADC 单个 Rank 转换完成                                                                                         */
    /* -------------------------------------------------------------------------------------------------------------- */

    if ((LL_ADC_IsActiveFlag_EOC(ADC1) != RESET) && (LL_ADC_IsEnabledIT_EOC(ADC1) != RESET))
    {

#if (ADC_USE_DMA == 0)

        uint16_t value;

        /*
         * 读取当前 Rank 的 12 bit ADC 转换结果。
         *
         * Rank 顺序：
         *
         * MOTOR -> WH -> SW -> USB -> NTC -> PGA2 -> VREFINT
         */
        value = LL_ADC_REG_ReadConversionData12(ADC1);

        /* 清除当前 Rank EOC 标志。 */
        LL_ADC_ClearFlag_EOC(ADC1);


        /*
         * 根据当前软件 Rank 索引，
         * 将转换结果保存到对应 ADC 数据缓存位置。
         */
        if (s_adcConvChannel < ADC_CH_NUM)
        {
            ADCxConvertedDatas[s_adcConvChannel] = value;
            s_adcConvChannel++;
        }


        /*
         * EOS 表示一整组规则序列转换完成。
         *
         * 当前即：
         *
         * Rank1 -> Rank2 -> Rank3 -> Rank4 -> Rank5 -> Rank6 -> Rank7
         *
         * 已全部转换完成。
         */
        if (LL_ADC_IsActiveFlag_EOS(ADC1) != RESET)
        {
            LL_ADC_ClearFlag_EOS(ADC1);

            /*
             * 下一次 TIM3 TRGO 触发时，
             * ADC 将重新从 Rank1 开始转换。
             */
            s_adcConvChannel = (adc_Channel_t)0U;

            /*
             * 通知应用层：
             * ADC 一整组通道数据已经更新完成。
             */
            bsp_adc_ChannelConvCpltCallback(ADCxConvertedDatas,ADC_CH_NUM);
        }

#endif /* ADC_USE_DMA == 0 */
    }
}


/*******************************************************************************
//  Function   : DMA1_Channel1_IRQHandler
//  Description: ADC DMA 中断服务函数
//  Param      : None
//  Return     : None
//  Others     : DMA 完成一组 ADC_CH_NUM 数据传输后调用 ADC 通道转换完成回调
//  Author     : Leo Chen 2026-08-18
*******************************************************************************/
void DMA1_Channel1_IRQHandler(void)
{
    /*
     * Block Transfer Complete：
     *
     * 表示本轮 ADC_CH_NUM 个转换结果已经全部通过 DMA
     * 从 ADC1->DR 搬运至 ADCxConvertedDatas[]。
     */
    if (LL_DMA_IsActiveFlag_BTC1(DMA1) == 1)
    {
        /* 清除 DMA Block Transfer Complete 标志。 */
        LL_DMA_ClearFlag_BTC1(DMA1);

#if ADC_USE_DMA

        /*
         * 通知应用层：
         * 一整组 ADC 数据已经更新完成。
         */
        bsp_adc_ChannelConvCpltCallback(ADCxConvertedDatas,ADC_CH_NUM);

#endif
    }
}
