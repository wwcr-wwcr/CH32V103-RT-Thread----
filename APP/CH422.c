/******************************************************************************
 * 文件名   : CH422.c
 * 芯片     : CH32V307VCT6
 * 说明     : 数码管应用层（APP）：I2C 时序 + 命令字 + 段码 + 显示
 *
 * 对外只有 1 个函数：CH422_Show，写法像 printf
 * 里面所有的东西都是 static，外面看不到，也不用管
 ******************************************************************************/

#include "CH422.h"
#include "BSP_CH422.h"
#include <stdio.h>
#include <stdarg.h>

/* CH422 的命令字 */
#define CH422_CMD_SYSTEM    0x48
#define CH422_SYS_VALUE     0x05
#define CH422_CMD_DIGIT1    0x70

/* 段码里的小数点 */
#define CH422_SEG_DP        0x20

/* 一次显示占几位 */
#define CH422_DIGITS        4

/* 0 = 还没初始化过，1 = 已经发过系统配置命令 */
static uint8_t s_ready = 0;

/**
 * @brief  I2C 的一个节拍延时
 * @note   想改通信速度就改这里的数字，单位微秒
 */
static void app_delay(void)
{
    BSP_CH422_DelayUs(2);
}

/**
 * @brief  发送起始条件：SCL 高电平期间，SDA 由高变低
 * @example
 *     app_i2c_start();
 *     app_i2c_write_byte(0x48);
 */
static void app_i2c_start(void)
{
    BSP_CH422_SDA_Output();

    BSP_CH422_SDA_H();
    BSP_CH422_SCL_H();
    app_delay();

    BSP_CH422_SDA_L();              // SCL 高时 SDA 下降，就是从机认的 START
    app_delay();

    BSP_CH422_SCL_L();
    app_delay();
}

/**
 * @brief  发送停止条件：SCL 高电平期间，SDA 由低变高
 * @example
 *     app_i2c_stop();
 */
static void app_i2c_stop(void)
{
    BSP_CH422_SDA_Output();

    BSP_CH422_SDA_L();
    app_delay();

    BSP_CH422_SCL_H();
    app_delay();

    BSP_CH422_SDA_H();              // SCL 高时 SDA 上升，就是从机认的 STOP
    app_delay();
}

/**
 * @brief  发送一个字节，高位先发，最后补第 9 个时钟
 * @param  data  要发的字节
 * @note   CH422 不会拉低应答，第 9 拍由主机把 SDA 保持高电平再打一拍
 * @example
 *     app_i2c_write_byte(0x70);
 */
static void app_i2c_write_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++)
    {
        if ((data & 0x80) != 0)
        {
            BSP_CH422_SDA_H();
        }
        else
        {
            BSP_CH422_SDA_L();
        }
        app_delay();

        BSP_CH422_SCL_H();          // 从机在上升沿采样
        app_delay();

        BSP_CH422_SCL_L();          // 低电平期间换下一位
        app_delay();

        data = (uint8_t)(data << 1);
    }

    BSP_CH422_SDA_H();              // 第 9 拍：主机输出 1
    app_delay();
    BSP_CH422_SCL_H();
    app_delay();
    BSP_CH422_SCL_L();
    app_delay();
}

/**
 * @brief  发一帧：命令 + 数据
 * @param  cmd    命令字，例如 0x48 或 0x70
 * @param  value  数据
 * @example
 *     app_write_frame(0x48, 0x05);      // 系统配置：打开输出和自动扫描
 *     app_write_frame(0x70, 0xD7);      // 第 1 位显示 0
 */
static void app_write_frame(uint8_t cmd, uint8_t value)
{
    app_i2c_start();
    app_i2c_write_byte(cmd);
    app_i2c_write_byte(value);
    app_i2c_stop();
}

/**
 * @brief  一个字符转成数码管的段码
 * @param  c  字符
 * @return 段码；不认识的字符返回空白
 * @note   本板段码顺序：bit0=a bit1=f bit2=b bit3=g bit4=c bit5=小数点 bit6=d bit7=e
 * @example
 *     uint8_t s = app_seg_of('8');      // s = 0xDF
 */
static uint8_t app_seg_of(char c)
{
    switch (c)
    {
        case '0': return 0xD7;
        case '1': return 0x14;
        case '2': return 0xCD;
        case '3': return 0x5D;
        case '4': return 0x1E;
        case '5': return 0x5B;
        case '6': return 0xDB;
        case '7': return 0x15;
        case '8': return 0xDF;
        case '9': return 0x5F;

        case '-': return 0x08;      // 中间那一横
        case ' ': return 0x00;      // 熄灭

        case 'A': case 'a': return 0x9F;
        case 'b': case 'B': return 0xDA;
        case 'C': case 'c': return 0xC3;
        case 'd': case 'D': return 0xDC;
        case 'E': case 'e': return 0xCB;
        case 'F': case 'f': return 0x8B;
        case 'H': case 'h': return 0x9E;
        case 'L': case 'l': return 0xC2;
        case 'P': case 'p': return 0x8F;
        case 'U': case 'u': return 0xD6;

        default:  return 0x00;
    }
}

/**
 * @brief  数码管显示（唯一对外的函数，写法像 printf）
 * @param  fmt  格式串，规则和 printf 一样
 * @param  ...  可变参数
 * @note   1. 只显示前 4 位，超出的直接丢掉
 * @note   2. 小数点会吃掉一位，所以 12.34 正好 4 位，再长就截断
 * @note   3. 第一次调用会自动初始化硬件，之后不再重复
 * @note   4. 多个任务同时调用会互相踩，迁到 RT-Thread 时外面要加互斥量
 * @example
 *     CH422_Show("%d", 1234);           // 显示 1234
 *     CH422_Show("%4d", 12);            // 右对齐，显示 "  12"
 *     CH422_Show("%d-%d", 1, 2);        // 显示 1-2
 *     CH422_Show("%d.%d", 25, 3);       // 显示 25.3
 *     CH422_Show("%d", -25);            // 显示 -25
 *     CH422_Show("    ");               // 四位全灭
 */
void CH422_Show(const char *fmt, ...)
{
    char    buf[24];
    uint8_t seg[CH422_DIGITS];
    uint8_t pos = 0;
    uint8_t i;
    va_list ap;

    if (fmt == 0)
    {
        return;
    }

    /* 先把格式串变成普通字符串，例如 "%d" 加 1234 变成 "1234" */
    va_start(ap, fmt);
    (void)vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    /* 第一次进来：初始化引脚 + 打开扫描 */
    if (s_ready == 0)
    {
        (void)BSP_CH422_Init();
        app_write_frame(CH422_CMD_SYSTEM, CH422_SYS_VALUE);
        s_ready = 1;
    }

    /* 四位先全部清空，避免上一次的残影 */
    for (i = 0; i < CH422_DIGITS; i++)
    {
        seg[i] = 0x00;
    }

    /* 逐字符摆到四位上，小数点贴到前一位 */
    for (i = 0; (buf[i] != '\0') && (pos < CH422_DIGITS); i++)
    {
        if (buf[i] == '.')
        {
            if (pos > 0)
            {
                seg[pos - 1] |= CH422_SEG_DP;
            }
            continue;
        }

        seg[pos] = app_seg_of(buf[i]);
        pos++;
    }

    /* 四位一次性刷出去 */
    for (i = 0; i < CH422_DIGITS; i++)
    {
        app_write_frame((uint8_t)(CH422_CMD_DIGIT1 + (i * 2)), seg[i]);
    }
}