/**
 ***********************************************************************************************************************
 * @file    cursor.h
 * @brief   数据缓冲区游标管理接口，实现数据写入、读取、跳过及指定位置访问
 * @author  Leo Chen
 * @version 1.1.0
 * @date    2026-11-13
 ***********************************************************************************************************************
 * @note
 *          1. 支持单字节、半字、字、数组及变量类型的数据操作。
 *          2. HALFWORD、WORD 操作按照 CURSOR_ENDIAN 配置的字节序执行。
 *          3. VAR 操作按照 MCU 内存字节序执行，不受 CURSOR_ENDIAN 影响。
 *          4. 起始偏移量由调用方设置，启动游标时不会自动清零。
 *          5. 调用方必须保证缓冲区容量、数据长度及指针合法。
 *          6. 偏移量必须为 uint16_t 类型，操作后不得超过 UINT16_MAX。
 *          7. DATA 操作使用 memcpy()，源区域与目标区域不得重叠。
 ***********************************************************************************************************************
 * @copyright Copyright (c) 1999-2025 ShenZhen H&T Control Technology Co., Ltd. All rights reserved.
 ***********************************************************************************************************************
 */

#ifndef _CURSOR_H_
#define _CURSOR_H_

/* Includes ----------------------------------------------------------------------------------------------------------*/

#include <stdint.h>
#include <string.h>

/* Configuration -----------------------------------------------------------------------------------------------------*/

/** 小端字节序：低字节存放在低地址 */
#define CURSOR_ENDIAN_LITTLE          (0U)

/** 大端字节序：高字节存放在低地址 */
#define CURSOR_ENDIAN_BIG             (1U)

/**
 * @brief 半字、字操作使用的字节序
 * @note  可在包含本文件前定义，或通过工程预定义宏配置。
 *        所有使用本文件的源文件应保持配置一致。
 */
#ifndef CURSOR_ENDIAN
#define CURSOR_ENDIAN                 CURSOR_ENDIAN_LITTLE
#endif

#if (CURSOR_ENDIAN != CURSOR_ENDIAN_LITTLE) && \
    (CURSOR_ENDIAN != CURSOR_ENDIAN_BIG)
#error "Invalid CURSOR_ENDIAN configuration"
#endif

/* Constants ---------------------------------------------------------------------------------------------------------*/

#define CURSOR_BYTE_SIZE              (1U)    /**< 字节：8 bit */
#define CURSOR_HALFWORD_SIZE          (2U)    /**< 半字：16 bit */
#define CURSOR_WORD_SIZE              (4U)    /**< 字：32 bit */

/* Internal Macros ---------------------------------------------------------------------------------------------------*/

/* i 为从 0 开始的字节索引，仅供本文件内部宏使用。 */
#if (CURSOR_ENDIAN == CURSOR_ENDIAN_LITTLE)

#define CURSOR_HALFWORD_SHIFT(i)      (8U * (i))
#define CURSOR_WORD_SHIFT(i)          (8U * (i))

#else

#define CURSOR_HALFWORD_SHIFT(i)      (8U * (1U - (i)))
#define CURSOR_WORD_SHIFT(i)          (8U * (3U - (i)))

#endif

/* Cursor Write Macro ------------------------------------------------------------------------------------------------*/

/**
 * @brief 启动数据缓冲区写入游标
 * @param p_dat 数据缓冲区首地址
 * @param p_len 当前写入偏移量变量，单位 Byte
 * @note  必须与 _STOP_CURSOR_WRITE() 配对使用。
 *        p_len 必须为 uint16_t 类型的可修改变量。
 */
#define _START_CURSOR_WRITE(p_dat, p_len)                      \
    do {                                                       \
        uint8_t *cursor = (uint8_t *)(p_dat);                  \
        uint16_t *offset = &(p_len);                           \
        (void)cursor;                                          \
        (void)offset;

/**
 * @brief 向缓冲区追加一个字节
 * @param byte 待写入的数据，转换为 uint8_t
 * @note  写入完成后偏移量增加 1。
 */
#define _CURSOR_WRITE_BYTE(byte)                               \
    do {                                                       \
        cursor[(*offset)++] = (uint8_t)(byte);                 \
    } while (0)

/**
 * @brief 向缓冲区追加一个半字
 * @param value 待写入的数据，转换为 uint16_t
 * @note  按 CURSOR_ENDIAN 配置的字节序写入。
 *        写入完成后偏移量增加 2。
 */
#define _CURSOR_WRITE_HALFWORD(value)                          \
    do {                                                       \
        uint16_t _cursor_value = (uint16_t)(value);            \
        uint16_t _cursor_pos = *offset;                        \
        uint32_t _cursor_i;                                    \
        for (_cursor_i = 0U;                                   \
             _cursor_i < CURSOR_HALFWORD_SIZE;                 \
             ++_cursor_i)                                      \
        {                                                      \
            cursor[_cursor_pos + _cursor_i] =                  \
                (uint8_t)((uint32_t)_cursor_value >>           \
                          CURSOR_HALFWORD_SHIFT(_cursor_i));   \
        }                                                      \
        *offset = (uint16_t)(_cursor_pos +                     \
                             CURSOR_HALFWORD_SIZE);            \
    } while (0)

/**
 * @brief 向缓冲区追加一个字
 * @param value 待写入的数据，转换为 uint32_t
 * @note  按 CURSOR_ENDIAN 配置的字节序写入。
 *        写入完成后偏移量增加 4。
 */
#define _CURSOR_WRITE_WORD(value)                               \
    do {                                                        \
        uint32_t _cursor_value = (uint32_t)(value);             \
        uint16_t _cursor_pos = *offset;                         \
        uint32_t _cursor_i;                                     \
        for (_cursor_i = 0U;                                    \
             _cursor_i < CURSOR_WORD_SIZE;                      \
             ++_cursor_i)                                       \
        {                                                       \
            cursor[_cursor_pos + _cursor_i] =                   \
                (uint8_t)(_cursor_value >>                      \
                          CURSOR_WORD_SHIFT(_cursor_i));        \
        }                                                       \
        *offset = (uint16_t)(_cursor_pos + CURSOR_WORD_SIZE);   \
    } while (0)

/**
 * @brief 写入一个占位字节
 * @note  在当前游标位置写入 0x00，偏移量增加 1。
 *        适用于预留协议长度、CRC 等字段。
 */
#define _CURSOR_WRITE_BYTE_NULL()                              \
    do {                                                       \
        cursor[(*offset)++] = 0U;                              \
    } while (0)

/**
 * @brief 修改已写入区域中指定位置的字节
 * @param pin  相对于缓冲区首地址的字节索引
 * @param byte 待写入的数据
 * @note  仅当 pin 小于当前偏移量时执行写入。
 *        不修改当前偏移量。
 */
#define _CURSOR_WRITE_BYTE_SPECIFY(pin, byte)                  \
    do {                                                       \
        uint16_t _cursor_pin = (uint16_t)(pin);                \
        if (_cursor_pin < (*offset))                           \
        {                                                      \
            cursor[_cursor_pin] = (uint8_t)(byte);             \
        }                                                      \
    } while (0)

/**
 * @brief 向缓冲区追加指定长度的数据
 * @param dat 待写入数据的首地址
 * @param len 数据长度，单位 Byte，范围 0~UINT16_MAX
 * @note  写入完成后偏移量增加 len。
 *        dat 与目标区域不得重叠。
 */
#define _CURSOR_WRITE_DATA(dat, len)                            \
    do {                                                        \
        uint16_t _cursor_len = (uint16_t)(len);                 \
        memcpy(&cursor[*offset], (dat), _cursor_len);           \
        *offset = (uint16_t)(*offset + _cursor_len);            \
    } while (0)

/**
 * @brief 将变量的内存数据追加至缓冲区
 * @param var 待写入变量，必须为可取地址的对象
 * @note  自动根据 sizeof(var) 确定写入长度。
 *        使用 MCU 内存字节序，不受 CURSOR_ENDIAN 影响。
 *        sizeof(var) 及写入后的偏移量不得超过 UINT16_MAX。
 *        不建议直接用于协议结构体，以免受到填充字节影响。
 */
#define _CURSOR_WRITE_VAR(var)                                 \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)sizeof(var);          \
        memcpy(&cursor[*offset], &(var), _cursor_len);         \
        *offset = (uint16_t)(*offset + _cursor_len);           \
    } while (0)

/**
 * @brief 结束数据缓冲区写入操作
 * @note  必须与 _START_CURSOR_WRITE() 配对使用。
 */
#define _STOP_CURSOR_WRITE()                                   \
    } while (0)

/* Cursor Read Macro -------------------------------------------------------------------------------------------------*/

/**
 * @brief 启动数据缓冲区读取游标
 * @param p_dat 数据缓冲区首地址
 * @param p_len 当前读取偏移量变量，单位 Byte
 * @note  必须与 _STOP_CURSOR_READ() 配对使用。
 *        p_len 必须为 uint16_t 类型的可修改变量。
 */
#define _START_CURSOR_READ(p_dat, p_len)                       \
    do {                                                       \
        const uint8_t *cursor = (const uint8_t *)(p_dat);      \
        uint16_t *offset = &(p_len);                           \
        (void)cursor;                                          \
        (void)offset;

/**
 * @brief 从当前游标位置读取一个字节
 * @param byte 接收结果的可修改变量，建议使用 uint8_t
 * @note  读取完成后偏移量增加 1。
 */
#define _CURSOR_READ_BYTE(byte)                                \
    do {                                                       \
        (byte) = cursor[(*offset)++];                          \
    } while (0)

/**
 * @brief 从当前游标位置读取一个半字
 * @param value 接收结果的可修改变量，建议使用 uint16_t
 * @note  按 CURSOR_ENDIAN 配置的字节序读取。
 *        读取完成后偏移量增加 2。
 */
#define _CURSOR_READ_HALFWORD(value)                           \
    do {                                                       \
        uint32_t _cursor_value = 0U;                           \
        uint16_t _cursor_pos = *offset;                        \
        uint32_t _cursor_i;                                    \
        for (_cursor_i = 0U;                                   \
             _cursor_i < CURSOR_HALFWORD_SIZE;                 \
             ++_cursor_i)                                      \
        {                                                      \
            _cursor_value |=                                   \
                (uint32_t)cursor[_cursor_pos + _cursor_i]      \
                << CURSOR_HALFWORD_SHIFT(_cursor_i);           \
        }                                                      \
        (value) = (uint16_t)_cursor_value;                     \
        *offset = (uint16_t)(_cursor_pos +                     \
                             CURSOR_HALFWORD_SIZE);            \
    } while (0)

/**
 * @brief 从当前游标位置读取一个字
 * @param value 接收结果的可修改变量，建议使用 uint32_t
 * @note  按 CURSOR_ENDIAN 配置的字节序读取。
 *        读取完成后偏移量增加 4。
 */
#define _CURSOR_READ_WORD(value)                               \
    do {                                                       \
        uint32_t _cursor_value = 0U;                           \
        uint16_t _cursor_pos = *offset;                        \
        uint32_t _cursor_i;                                    \
        for (_cursor_i = 0U;                                   \
             _cursor_i < CURSOR_WORD_SIZE;                     \
             ++_cursor_i)                                      \
        {                                                      \
            _cursor_value |=                                   \
                (uint32_t)cursor[_cursor_pos + _cursor_i]      \
                << CURSOR_WORD_SHIFT(_cursor_i);               \
        }                                                      \
        (value) = _cursor_value;                               \
        *offset = (uint16_t)(_cursor_pos + CURSOR_WORD_SIZE);  \
    } while (0)

/**
 * @brief 跳过一个字节
 * @note  不读取缓冲区数据，仅将偏移量增加 1。
 */
#define _CURSOR_READ_BYTE_NULL()                               \
    do {                                                       \
        (*offset)++;                                           \
    } while (0)

/**
 * @brief 读取指定位置的一个字节
 * @param pin  相对于缓冲区首地址的字节索引
 * @param byte 接收结果的可修改变量
 * @note  仅当 pin 小于当前偏移量时执行读取。
 *        条件不满足时，接收变量保持原值。
 *        不修改当前偏移量。
 */
#define _CURSOR_READ_BYTE_SPECIFY(pin, byte)                   \
    do {                                                       \
        uint16_t _cursor_pin = (uint16_t)(pin);                \
        if (_cursor_pin < (*offset))                           \
        {                                                      \
            (byte) = cursor[_cursor_pin];                      \
        }                                                      \
    } while (0)

/**
 * @brief 从缓冲区读取指定长度的数据
 * @param dat 接收数据的缓冲区首地址
 * @param len 数据长度，单位 Byte，范围 0~UINT16_MAX
 * @note  读取完成后偏移量增加 len。
 *        dat 与源区域不得重叠。
 */
#define _CURSOR_READ_DATA(dat, len)                            \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)(len);                \
        memcpy((dat), &cursor[*offset], _cursor_len);          \
        *offset = (uint16_t)(*offset + _cursor_len);           \
    } while (0)

/**
 * @brief 从缓冲区读取变量的内存数据
 * @param var 接收结果的变量，必须为可取地址且可修改的对象
 * @note  自动根据 sizeof(var) 确定读取长度。
 *        使用 MCU 内存字节序，不受 CURSOR_ENDIAN 影响。
 *        sizeof(var) 及读取后的偏移量不得超过 UINT16_MAX。
 *        调用方应保证数据为目标类型的有效内存表示。
 */
#define _CURSOR_READ_VAR(var)                                  \
    do {                                                       \
        uint16_t _cursor_len = (uint16_t)sizeof(var);          \
        memcpy(&(var), &cursor[*offset], _cursor_len);         \
        *offset = (uint16_t)(*offset + _cursor_len);           \
    } while (0)

/**
 * @brief 结束数据缓冲区读取操作
 * @note  必须与 _START_CURSOR_READ() 配对使用。
 */
#define _STOP_CURSOR_READ()                                   \
    } while (0)

#endif /* _CURSOR_H_ */
