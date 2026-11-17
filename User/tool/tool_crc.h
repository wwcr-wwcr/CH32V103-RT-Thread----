/******************************************************************************
 * 文件名   : tool_crc.h
 * 说明     : 校验 —— CRC8 / CRC16-MODBUS / 累加和
 *
 * 串口协议、EEPROM 存参数、无线包校验都用得上。
 * 这里全是"逐位算"的版本，不带表：省 512 字节 Flash，速度对 115200/921600
 * 的包长完全够（1KB 数据约 20~40us）。
 *
 * 标准测试向量（"123456789"，9 字节）：
 *     tool_crc8()           -> 0xF4
 *     tool_crc16_modbus()   -> 0x4B37（低字节先发：0x37 0x4B）
 *
 * 用法:
 *     uint16_t c = tool_crc16_modbus(buf, len);
 *     buf[len]     = (uint8_t)(c & 0xFF);
 *     buf[len + 1] = (uint8_t)(c >> 8);
 ******************************************************************************/

#ifndef __TOOL_CRC_H
#define __TOOL_CRC_H

#include <stdint.h>

/**
 * @brief  CRC-8（多项式 0x07，初值 0x00，不反转、不异或）
 * @param  data  数据，可传 0（长度非 0 时返回 0）
 * @param  len   字节数
 * @note   就是常见的 CRC-8/ATM，很多传感器、Dallas 的变种都从这里改
 */
static inline uint8_t tool_crc8(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t  crc = 0x00u;
    uint16_t i;
    uint8_t  b;

    if((p == 0) || (len == 0u)) { return 0u; }

    for(i = 0; i < len; i++)
    {
        crc ^= p[i];

        for(b = 0; b < 8u; b++)
        {
            crc = (uint8_t)((crc & 0x80u) ? ((uint32_t)(crc << 1) ^ 0x07u)
                                         : (uint32_t)(crc << 1));
        }
    }

    return crc;
}

/**
 * @brief  CRC-16/MODBUS（多项式 0xA001 反转，初值 0xFFFF）
 * @param  data  数据，可传 0（长度非 0 时返回 0）
 * @param  len   字节数
 * @note   发出去时低字节在前，和 Modbus RTU 一致
 */
static inline uint16_t tool_crc16_modbus(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint16_t crc = 0xFFFFu;
    uint16_t i;
    uint8_t  b;

    if((p == 0) || (len == 0u)) { return 0u; }

    for(i = 0; i < len; i++)
    {
        crc ^= p[i];

        for(b = 0; b < 8u; b++)
        {
            crc = (crc & 1u) ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
        }
    }

    return crc;
}

/**
 * @brief  累加和（所有字节相加取低 8 位）
 * @note   最简单、最不抗错的校验，老式红外/串口模块常见，别用在关键数据上
 */
static inline uint8_t tool_sum8(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    uint8_t  s = 0u;
    uint16_t i;

    if(p == 0) { return 0u; }

    for(i = 0; i < len; i++) { s = (uint8_t)(s + p[i]); }

    return s;
}

#endif
