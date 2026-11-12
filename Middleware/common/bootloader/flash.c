
/**
 ***********************************************************************************************************************
 * @file    flash.c
 * @brief   Flash 存储器底层操作，实现 Flash 编程、Option Bytes 写入及擦除功能
 * @author  Puya Application Team
 ***********************************************************************************************************************
 * @attention
 *
 * Copyright (c) 2021 Puya Semiconductor.
 * All rights reserved.
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */


/* Includes ----------------------------------------------------------------------------------------------------------*/

#include "main.h"
#include "flash.h"
#include "bootloader.h"


/* Function implementations ------------------------------------------------------------------------------------------*/

/*******************************************************************************
//  Function   : WriteFlash
//  Description: 将指定数据写入 Flash 存储器
//  Param      : dwAddr       - Flash 目标写入地址
//               pucDataBuf   - 待写入数据缓冲区首地址
//               ucDataLength - 数据长度编码，实际长度为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - Flash 编程成功
//               RET_ERROR_NACK  - 参数错误、写保护或 Flash 编程失败
//  Others     : Flash 以 128 Byte 为一页，每页包含 32 个 32 位数据字；
//               ADJUST_ADDR_AND_SIZE 使能时自动调整地址及长度；
//               对未覆盖的页内数据，从原 Flash 中读取并补齐；
//               CHECK_WRP 使能时检查目标扇区写保护状态；
//               Flash 解锁及初始空闲状态检查由外部流程负责；
//               本函数不执行 Flash 擦除及写入数据回读校验
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus WriteFlash(uint32_t dwAddr,
                       uint8_t *pucDataBuf,
                       uint8_t ucDataLength)
{
#ifdef CHECK_WRP
    uint8_t ucSector;
#endif

    uint16_t i, j;
    uint16_t wDataLength;
    uint32_t dwOffsetAddr;
    ErrorStatus eResultFlag;

#ifdef ADJUST_ADDR_AND_SIZE

    uint32_t dwOrgAddr;       /* 保存调整前的 Flash 起始地址。 */
    uint32_t dwEndAddr;       /* 保存调整后的 Flash 结束地址。 */

    /* 将协议长度编码转换为实际数据长度。 */
    wDataLength = ucDataLength + 1;

    /*
     * Flash 页对齐处理：
     *
     * Flash 每页大小为 128 Byte，编程地址及编程长度
     * 需要按照 Flash 页大小进行对齐。
     *
     * 原厂 uISP 工具对齐示例：
     *
     * 原始地址       数据长度    调整后起始地址    调整后结束地址    调整后长度
     * 0x08000000    0xC8        0x08000000       0x080000FF       0x100
     * 0x080000C8    0xC8        0x08000080       0x080001FF       0x180
     * 0x08000190    0xC8        0x08000180       0x0800027F       0x100
     *
     * 未覆盖区域需要读取原 Flash 内容进行补齐。
     * 调用方必须确保 pucDataBuf 具有足够的可写空间。
     */

    /* 保存调整前的 Flash 写入地址。 */
    dwOrgAddr = dwAddr;

    /* 检查起始地址是否满足 128 Byte 页对齐要求。 */
    if (dwAddr & 0x7F)
    {
        /* 清除低 7 位，将起始地址向下对齐至页边界。 */
        CLEAR_BIT(dwAddr, 0x7F);

        /*
         * 将原始数据整体向后移动。
         *
         * 移动距离为：
         *     dwOrgAddr - dwAddr
         * 即：
         *     dwOrgAddr & 0x7F
         *
         * 采用从尾部向首部复制的方式，
         * 避免移动过程中覆盖尚未复制的数据。
         */
        for (i = 0; i < wDataLength; i++)
        {
            pucDataBuf[(dwOrgAddr & 0x7F) + wDataLength - 1 - i] =
                pucDataBuf[wDataLength - 1 - i];
        }

        /*
         * 补齐调整后的起始地址与原始地址之间的数据。
         *
         * 该区域不属于本次更新范围，
         * 因此从原 Flash 中读取数据并保存至缓冲区前部。
         *
         * 必须先移动原始数据，再读取 Flash 前缀数据，
         * 防止覆盖待编程数据。
         */
        for (i = 0; i < (dwOrgAddr & 0x7F); i++)
        {
            pucDataBuf[i] = M8(dwAddr + i);
        }
    }

    /*
     * 当原始起始地址或数据长度未按照 128 Byte 对齐时，
     * 继续调整编程结束地址及编程数据长度。
     */
    if ((dwOrgAddr & 0x7F) || (wDataLength & 0x7F))
    {
        /* 根据原有地址计算表达式获取调整后的结束地址。 */
        dwEndAddr =
            ((dwOrgAddr + wDataLength - 1 + 0x7F) & ~0x7F) - 1;

        /*
         * 将原始数据末尾至调整后结束地址之间的内容，
         * 从 Flash 中读取并补齐至数据缓冲区。
         */
        for (i = ((dwOrgAddr & 0x7F) + wDataLength);
             i < (dwEndAddr - dwAddr + 1);
             i++)
        {
            pucDataBuf[i] = M8(dwAddr + i);
        }

        /* 更新对齐后的 Flash 编程总长度。 */
        wDataLength = dwEndAddr - dwAddr + 1;
    }

#else

    /* 将协议长度编码转换为实际数据长度。 */
    wDataLength = ucDataLength + 1;

    /*
     * 未启用自动对齐功能时：
     *
     * 1. Flash 写入地址必须按照 128 Byte 对齐。
     * 2. Flash 写入长度必须为 128 Byte 的整数倍。
     *
     * 任意条件不满足时返回错误应答。
     */
    if ((wDataLength & 0x7F) || (dwAddr & 0x7F))
    {
        return RET_ERROR_NACK;
    }

#endif

    /*
     * Flash 编程前置条件：
     *
     * 1. FLASH_SR.BSY 应处于清零状态。
     * 2. FLASH_CR 应已通过 KEY1、KEY2 解锁。
     * 3. 需要保留的数据应提前备份。
     * 4. 目标 Flash 页应满足芯片编程要求。
     *
     * 当前函数未直接执行上述全部检查和解锁操作，
     * 由外部 Flash 操作流程负责保证。
     */

    /*
     * 使能 Flash 编程模式及操作结束中断：
     *
     * PG    ：使能 Flash 编程。
     * EOPIE ：使能 Flash 操作结束中断。
     */
    SET_BIT(FLASH->CR, (FLASH_CR_PG | FLASH_CR_EOPIE));

    /* 初始化 Flash 编程地址偏移量。 */
    dwOffsetAddr = 0;

    /* 按 128 Byte 为一页依次执行 Flash 编程。 */
    for (i = 0; i < wDataLength / 0x80; i++)
    {

#ifdef CHECK_WRP

        /*
         * 计算当前编程地址所属 Flash 扇区。
         *
         * 原厂实现按照 0x1000 Byte，即 4KB，
         * 作为写保护检查的扇区划分单位。
         */
        ucSector = (dwAddr + dwOffsetAddr - FLASH_BASE) / 0x1000;

        /*
         * 检查当前扇区写保护状态。
         *
         * 对应 WRPR 位为 0 时，
         * 当前代码认为该扇区禁止写入。
         */
        if (0 == READ_BIT(FLASH->WRPR, (1U << ucSector)))
        {
            eResultFlag = RET_ERROR_NACK;
            break;
        }

#endif

        /*
         * 每页包含 32 个 32 位数据字：
         *
         * 32 Word × 4 Byte = 128 Byte。
         *
         * Flash 编程数据按照 32 位访问方式写入。
         */
        for (j = 0; j < 0x20; j++)
        {
            /* 将当前 32 位数据写入对应 Flash 地址。 */
            M32(dwAddr + dwOffsetAddr) =
                M32(pucDataBuf + dwOffsetAddr);

            /* 移动至下一个 32 位数据位置。 */
            dwOffsetAddr += 4;

            /*
             * 当第 31 个 Word 写入完成后，
             * 置位 FLASH_CR.PGSTRT。
             *
             * 此时 j = 30，随后继续写入第 32 个 Word。
             */
            if ((0x20 - 2) == j)
            {
                SET_BIT(FLASH->CR, FLASH_CR_PGSTRT);
            }
        }

        /*
         * 第 32 个 Word 写入完成后，
         * 等待 Flash 控制器结束当前编程操作。
         */
        while (FLASH_SR_BSY ==
               READ_BIT(FLASH->SR, FLASH_SR_BSY));

        /*
         * 检查 Flash 操作结束标志。
         *
         * EOP 置位表示当前操作结束，
         * 原厂代码据此判断本次编程操作是否成功。
         */
        if (FLASH_SR_EOP ==
            READ_BIT(FLASH->SR, FLASH_SR_EOP))
        {
            eResultFlag = RET_SUCCESS_ACK;

            /* EOP 为写 1 清零标志。 */
            SET_BIT(FLASH->SR, FLASH_SR_EOP);
        }
        else
        {
            /* 未检测到 EOP 标志，终止后续编程。 */
            eResultFlag = RET_ERROR_NACK;
            break;
        }
    }

    /*
     * 全部 Flash 编程操作结束后，
     * 关闭编程模式及操作结束中断。
     */
    CLEAR_BIT(FLASH->CR, (FLASH_CR_EOPIE | FLASH_CR_PG));

    /* 返回 Flash 编程执行结果。 */
    return eResultFlag;
}


/*******************************************************************************
//  Function   : WriteOption
//  Description: 写入 Flash Option Bytes 配置
//  Param      : dwAddr       - Option Bytes 目标写入地址
//               pucDataBuf   - 待写入配置数据缓冲区
//               ucDataLength - 数据长度编码，实际长度为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - Option Bytes 写入成功
//               RET_ERROR_NACK  - Option Bytes 写入失败
//  Others     : 首先读取当前 OPTR、WRPR 及 PCROPR 寄存器；
//               根据输入地址更新对应配置字节；
//               未指定修改的配置位保持原值；
//               写入成功后设置 OBL_LAUNCH，重新加载 Option Bytes；
//               Option Bytes 配置变更可能影响芯片启动及保护状态
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus WriteOption(uint32_t dwAddr,
                        uint8_t *pucDataBuf,
                        uint8_t ucDataLength)
{
    ErrorStatus eResultFlag;
    uint16_t i;

    /* 读取当前 Option Bytes 配置，保留未修改的字段。 */
    uint32_t dwOPTR    = FLASH->OPTR;
    uint32_t dwWRPR    = FLASH->WRPR;
    uint32_t dwPCROPR0 = FLASH->PCROPR;

    /*
     * Option Bytes 编程前置条件：
     *
     * FLASH_SR.BSY 应处于清零状态，
     * FLASH 主控制寄存器应已经解锁。
     *
     * 当前函数仅执行 Option Bytes 专用解锁操作。
     */

    /* 检查 Option Bytes 是否处于锁定状态。 */
    if (READ_BIT(FLASH->CR, FLASH_CR_OPTLOCK) != 0x00U)
    {
        /*
         * 依次向 OPTKEYR 写入两组解锁密钥，
         * 解除 Option Bytes 写入保护。
         */
        WRITE_REG(FLASH->OPTKEYR, FLASH_OPTKEY1);
        WRITE_REG(FLASH->OPTKEYR, FLASH_OPTKEY2);
    }

    /*
     * 根据 Option Bytes 地址逐字节更新配置。
     *
     * ucDataLength 为协议长度编码，
     * 实际处理的字节数量为 ucDataLength + 1。
     */
    for (i = 0; i < ucDataLength + 1; i++)
    {
        /*
         * ================================================================
         * OPTR 配置寄存器更新
         * ================================================================
         */

        /* OPTR 第 0 字节：RDP 读保护配置。 */
        if ((OPTR1_BASE + 0) == (dwAddr + i))
        {
            dwOPTR &= ~(0x000000FF);
            dwOPTR |= (pucDataBuf[i] << 0);
        }
        /* OPTR 第 1 字节：BOR、看门狗及启动相关配置。 */
        else if ((OPTR1_BASE + 1) == (dwAddr + i))
        {
            dwOPTR &= ~(0x0000FF00);
            dwOPTR |= (pucDataBuf[i] << 8);
        }
        /* OPTR 第 2 字节：对应 OPTR2_BASE 的低字节。 */
        else if ((OPTR2_BASE + 0) == (dwAddr + i))
        {
            dwOPTR &= ~(0x00FF0000);
            dwOPTR |= (pucDataBuf[i] << 16);
        }
        /* OPTR 第 3 字节：对应 OPTR2_BASE 的高字节。 */
        else if ((OPTR2_BASE + 1) == (dwAddr + i))
        {
            dwOPTR &= ~(0xFF000000);
            dwOPTR |= (pucDataBuf[i] << 24);
        }

        /*
         * ================================================================
         * WRPR 写保护配置寄存器更新
         * ================================================================
         */

        /* WRPR Bit[7:0]：低 8 位写保护配置。 */
        if ((WRPR_BASE + 0) == (dwAddr + i))
        {
            dwWRPR &= ~(0x000000FF);
            dwWRPR |= (pucDataBuf[i] << 0);
        }
        /* WRPR Bit[15:8]：高 8 位写保护配置。 */
        else if ((WRPR_BASE + 1) == (dwAddr + i))
        {
            dwWRPR &= ~(0x0000FF00);
            dwWRPR |= (pucDataBuf[i] << 8);
        }

        /*
         * ================================================================
         * PCROPR 专有代码读保护配置寄存器更新
         * ================================================================
         */

        /* PCROP0SR 低字节，对应 PCROPR Bit[7:0]。 */
        if ((PCROP0SR_BASE + 0) == (dwAddr + i))
        {
            dwPCROPR0 &= ~(0x000000FF);
            dwPCROPR0 |= (pucDataBuf[i] << 0);
        }
        /* PCROP0SR 高字节，对应 PCROPR Bit[15:8]。 */
        else if ((PCROP0SR_BASE + 1) == (dwAddr + i))
        {
            dwPCROPR0 &= ~(0x0000FF00);
            dwPCROPR0 |= (pucDataBuf[i] << 8);
        }
        /* PCROP0ER 低字节，对应 PCROPR Bit[23:16]。 */
        else if ((PCROP0ER_BASE + 0) == (dwAddr + i))
        {
            dwPCROPR0 &= ~(0x00FF0000);
            dwPCROPR0 |= (pucDataBuf[i] << 16);
        }
        /* PCROP0ER 高字节，对应 PCROPR Bit[31:24]。 */
        else if ((PCROP0ER_BASE + 1) == (dwAddr + i))
        {
            dwPCROPR0 &= ~(0xFF000000);
            dwPCROPR0 |= (pucDataBuf[i] << 24);
        }
    }

    /*
     * 将更新后的配置写入 Option Bytes 对应控制寄存器。
     *
     * OPTR   ：读保护、BOR、看门狗及启动相关配置。
     * WRPR   ：Flash 写保护配置。
     * PCROPR ：专有代码读保护区域配置。
     */
    FLASH->OPTR   = dwOPTR;
    FLASH->WRPR   = dwWRPR;
    FLASH->PCROPR = dwPCROPR0;

    /*
     * 使能 Option Bytes 编程及操作结束中断，
     * 并按照原厂操作流程向 Option Bytes 地址写入数据，
     * 触发对应配置写入操作。
     */
    SET_BIT(FLASH->CR, (FLASH_CR_EOPIE | FLASH_CR_OPTSTRT));
    M32(0x1FFF0F80) = 0xFF;

    /* 等待 Option Bytes 编程操作完成。 */
    while (FLASH_SR_BSY ==
           READ_BIT(FLASH->SR, FLASH_SR_BSY));

    /* 根据 EOP 标志获取 Option Bytes 写入结果。 */
    eResultFlag =
        (FLASH_SR_EOP == READ_BIT(FLASH->SR, FLASH_SR_EOP))
        ? RET_SUCCESS_ACK
        : RET_ERROR_NACK;

    /* 清除 Flash 操作结束标志，EOP 为写 1 清零。 */
    SET_BIT(FLASH->SR, FLASH_SR_EOP);

    /* 关闭 Option Bytes 编程及操作结束中断。 */
    CLEAR_BIT(FLASH->CR, (FLASH_CR_EOPIE | FLASH_CR_OPTSTRT));

    /*
     * Option Bytes 锁定说明：
     *
     * 当前保留原厂注释中的 OPTLOCK 操作。
     * 原代码说明主 Flash LOCK 置位时，
     * OPTLOCK 也会自动置位。
     */

    //SET_BIT(FLASH->CR, FLASH_CR_OPTLOCK);

    /* Option Bytes 写入成功后执行重新加载。 */
    if (RET_SUCCESS_ACK == eResultFlag)
    {
        /*
         * 原厂代码预留串口成功应答发送接口。
         *
         * 当前该接口未启用。
         */

        //USART_SendByte(ACK_BYTE);

        /*
         * 设置 OBL_LAUNCH，触发 Option Bytes 重新加载。
         *
         * 该操作可能导致系统复位，
         * 因此应在执行前完成必要的通信应答及状态保存。
         */
        SET_BIT(FLASH->CR, FLASH_CR_OBL_LAUNCH);
    }

    /* 返回 Option Bytes 编程执行结果。 */
    return eResultFlag;
}


/*******************************************************************************
//  Function   : MassErase
//  Description: 执行 Flash 整片擦除操作
//  Param      : None
//  Return     : RET_SUCCESS_ACK - Flash 整片擦除成功
//               RET_ERROR_NACK  - Flash 整片擦除失败
//  Others     : 置位 FLASH_CR.MER，进入整片擦除模式；
//               向 Flash 地址写入 32 位数据触发擦除；
//               等待操作完成并检查 EOP 标志；
//               本函数不负责 Flash 解锁及初始空闲状态检查；
//               实际擦除范围受芯片存储结构及保护配置影响
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus MassErase(void)
{
    ErrorStatus eResultFlag;

    /*
     * 原厂代码预留的 Flash 写保护检查。
     *
     * 当前检查逻辑已被注释，不参与实际执行。
     */

//  if (FLASH_WRPR_WRP != FLASH->WRPR)
//  {
//      return RET_ERROR_NACK;
//  }

    /*
     * 原厂代码预留的保护区域检查。
     *
     * 当前检查逻辑已被注释，不参与实际执行。
     */

//  if ((FLASH->SDKR & 0x1F) <= ((FLASH->SDKR >> 8) & 0x1F))
//  {
//      return RET_ERROR_NACK;
//  }

    /*
     * 整片擦除前置条件：
     *
     * 1. FLASH_SR.BSY 应处于清零状态。
     * 2. FLASH_CR 应已经完成解锁。
     * 3. 调用方应确认整片擦除的实际影响范围。
     */

    /* 置位 MER，进入 Flash 整片擦除模式。 */
    SET_BIT(FLASH->CR, (FLASH_CR_MER));

    /*
     * 向 Flash 任意有效地址写入 32 位数据，
     * 触发整片擦除操作。
     *
     * 当前选择 FLASH_BASE 作为触发地址。
     */
    M32(FLASH_BASE) = 0xFFFFFFFF;

    /* 等待 Flash 整片擦除操作完成。 */
    while (FLASH_SR_BSY ==
           READ_BIT(FLASH->SR, FLASH_SR_BSY));

    /* 根据 EOP 标志判断擦除操作结果。 */
    eResultFlag =
        (FLASH_SR_EOP == READ_BIT(FLASH->SR, FLASH_SR_EOP))
        ? RET_SUCCESS_ACK
        : RET_ERROR_NACK;

    /* 清除 Flash 操作结束标志。 */
    SET_BIT(FLASH->SR, FLASH_SR_EOP);

    /* 清除 MER，退出 Flash 整片擦除模式。 */
    CLEAR_BIT(FLASH->CR, (FLASH_CR_MER));

    /* 返回整片擦除执行结果。 */
    return eResultFlag;
}


/*******************************************************************************
//  Function   : PageErase
//  Description: 根据指定页组信息执行 Flash 页擦除
//  Param      : dwAddr    - 待擦除页地址
//  Return     : RET_SUCCESS_ACK - 指定 Flash 页擦除成功
//               RET_ERROR_NACK  - Flash 写保护或擦除失败
//  Others     : 原厂实现使用 128 Byte 作为 Flash 页大小；
//               根据页组编号及组内页索引计算目标页地址；
//               CHECK_WRP 使能时检查对应扇区写保护；
//               擦除过程中发生错误时终止后续擦除；
//               Flash 解锁及初始空闲状态检查由外部流程负责
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus PageErase(uint32_t dwAddr)
{

#ifdef CHECK_WRP
    uint8_t ucSector;
#endif


    ErrorStatus eResultFlag = RET_SUCCESS_ACK;

    /*
     * Flash 页擦除前置条件：
     *
     * 1. FLASH_SR.BSY 应处于清零状态。
     * 2. FLASH_CR 应已经完成解锁。
     * 3. 待擦除页组编号及页地址应处于有效 Flash 范围。
     *
     * 当前函数未直接执行全部前置条件检查。
     */

    /* 置位 PER，进入 Flash 页擦除模式。 */
    SET_BIT(FLASH->CR, (FLASH_CR_PER));

	/*
	 * 原厂代码预留 Flash 容量边界检查。
	 *
	 * 当前该检查已被注释，
	 * 不会对目标地址执行容量限制。
	 */

//   if ((((M8(FLASHSIZE_BASE) & 0x07) >> 0) + 1) * 8 * 0x400
//       <= (dwAddr - FLASH_BASE))
//   {
//       eResultFlag = RET_SUCCESS_ACK;
//       continue;
//   }

#ifdef CHECK_WRP

	/*
	 * 计算目标页所属的 4KB Flash 扇区，
	 * 并检查对应写保护配置。
	 */
	ucSector = (dwAddr - FLASH_BASE) / 0x1000;

	/* 当前扇区处于写保护状态时终止擦除。 */
	if (0 == READ_BIT(FLASH->WRPR, (1U << ucSector)))
	{
		eResultFlag = RET_ERROR_NACK;
		break;
	}

#endif

	/*
	 * 向目标 Flash 页写入任意 32 位数据，
	 * 触发当前页的擦除操作。
	 */
	M32(dwAddr) = 0xFFFFFFFF;

	/* 等待当前 Flash 页擦除完成。 */
	while (FLASH_SR_BSY ==
		   READ_BIT(FLASH->SR, FLASH_SR_BSY));

	/* 检查 Flash 操作结束标志。 */
	if (FLASH_SR_EOP ==
		READ_BIT(FLASH->SR, FLASH_SR_EOP))
	{
		eResultFlag = RET_SUCCESS_ACK;

		/* 清除 EOP 标志，准备下一页擦除。 */
		SET_BIT(FLASH->SR, FLASH_SR_EOP);
	}
	else
	{
		/* 当前页擦除失败，终止组内剩余页操作。 */
		eResultFlag = RET_ERROR_NACK;
	}

	/* 清除 PER，退出 Flash 页擦除模式。 */
	CLEAR_BIT(FLASH->CR, (FLASH_CR_PER));

    /* 返回 Flash 页擦除执行结果。 */
    return eResultFlag;
}


/*******************************************************************************
//  Function   : SectorErase
//  Description: 根据指定扇区编号执行 Flash 扇区擦除
//  Param      : pwDataBuf    - 待擦除 Flash 扇区编号缓冲区
//               ucDataLength - 扇区数量编码，实际数量为 ucDataLength + 1
//  Return     : RET_SUCCESS_ACK - 指定 Flash 扇区擦除成功
//               RET_ERROR_NACK  - Flash 写保护或扇区擦除失败
//  Others     : 原厂实现使用 0x1000 Byte，即 4KB 作为扇区大小；
//               根据扇区编号计算 Flash 擦除起始地址；
//               CHECK_WRP 使能时检查扇区写保护状态；
//               擦除过程中发生错误时终止后续操作；
//               Flash 解锁及初始空闲状态检查由外部流程负责
//  Author     : Puya Application Team
*******************************************************************************/
ErrorStatus SectorErase(uint16_t *pwDataBuf,
                        uint8_t ucDataLength)
{
    uint8_t i;
    ErrorStatus eResultFlag;

    /*
     * Flash 扇区擦除前置条件：
     *
     * 1. FLASH_SR.BSY 应处于清零状态。
     * 2. FLASH_CR 应已经完成解锁。
     * 3. 输入扇区编号必须处于有效范围。
     */

    /* 置位 SER，进入 Flash 扇区擦除模式。 */
    SET_BIT(FLASH->CR, (FLASH_CR_SER));

    /* 依次处理输入缓冲区中的待擦除扇区。 */
    for (i = 0; i < ucDataLength + 1; i++)
    {
        /*
         * 原厂代码预留看门狗刷新操作。
         * 当前未启用。
         */

        //WDG_Refresh();

#ifdef CHECK_WRP

        /* 检查当前 Flash 扇区是否处于写保护状态。 */
        if (0 == READ_BIT(FLASH->WRPR, (1U << pwDataBuf[i])))
        {
            eResultFlag = RET_ERROR_NACK;
            break;
        }

#endif

        /*
         * 根据扇区编号计算目标擦除地址：
         *
         * 地址 = FLASH_BASE + 扇区编号 × 4096 Byte。
         *
         * 向该地址写入 32 位数据触发扇区擦除。
         */
        M32(FLASH_BASE + pwDataBuf[i] * 0x1000) = 0xFFFFFFFF;

        /* 等待当前 Flash 扇区擦除操作完成。 */
        while (FLASH_SR_BSY ==
               READ_BIT(FLASH->SR, FLASH_SR_BSY));

        /* 检查 Flash 操作结束标志。 */
        if (FLASH_SR_EOP ==
            READ_BIT(FLASH->SR, FLASH_SR_EOP))
        {
            eResultFlag = RET_SUCCESS_ACK;

            /* 清除操作结束标志，准备下一扇区擦除。 */
            SET_BIT(FLASH->SR, FLASH_SR_EOP);
        }
        else
        {
            /* 当前扇区擦除失败，终止后续操作。 */
            eResultFlag = RET_ERROR_NACK;
            break;
        }
    }

    /* 清除 SER，退出 Flash 扇区擦除模式。 */
    CLEAR_BIT(FLASH->CR, (FLASH_CR_SER));

    /* 返回 Flash 扇区擦除执行结果。 */
    return eResultFlag;
}


/************************ (C) COPYRIGHT Puya Semiconductor *****END OF FILE****/
