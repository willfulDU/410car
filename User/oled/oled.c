#include "oled.h"
#include "font_matrix.h"
#include <stdint.h>

/* 简单延时，给模拟I2C一点时序余量 */
static void OLED_I2C_Delay(void)
{
    uint8_t i;
    for(i = 0; i < 10; i++);
}

/* 模拟 I2C 起始信号 */
static void OLED_I2C_Start(void)
{
    OLED_W_SDA(1);
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(0);
    OLED_I2C_Delay();
    OLED_W_SCL(0);
}

/* 模拟 I2C 停止信号 */
static void OLED_I2C_Stop(void)
{
    OLED_W_SDA(0);
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SDA(1);
    OLED_I2C_Delay();
}

/* I2C 发送一个字节 */
static void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for(i = 0; i < 8; i++)
    {
        OLED_W_SDA((Byte & (0x80 >> i)) ? 1 : 0);
        OLED_I2C_Delay();
        OLED_W_SCL(1);
        OLED_I2C_Delay();
        OLED_W_SCL(0);
        OLED_I2C_Delay();
    }

    /* 跳过ACK读取，只补一个时钟 */
    OLED_W_SDA(1);
    OLED_I2C_Delay();
    OLED_W_SCL(1);
    OLED_I2C_Delay();
    OLED_W_SCL(0);
    OLED_I2C_Delay();
}

/* 写 OLED 命令 */
static void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);      // 从机地址：0x3C << 1
    OLED_I2C_SendByte(0x00);      // 控制字：后面是命令
    OLED_I2C_SendByte(Command);
    OLED_I2C_Stop();
}

/* 写 OLED 数据 */
static void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_Start();
    OLED_I2C_SendByte(0x78);      // 从机地址：0x3C << 1
    OLED_I2C_SendByte(0x40);      // 控制字：后面是数据
    OLED_I2C_SendByte(Data);
    OLED_I2C_Stop();
}

/* 设置光标位置（页地址模式） */
static void OLED_SetCursor(uint8_t Page, uint8_t Column)
{
    OLED_WriteCommand(0xB0 | Page);                    // 页地址 0~7
    OLED_WriteCommand(0x10 | ((Column & 0xF0) >> 4)); // 列地址高4位
    OLED_WriteCommand(0x00 | (Column & 0x0F));        // 列地址低4位
}

/* 清屏 */
void OLED_Clear(void)
{
    uint8_t i, j;
    for(i = 0; i < 8; i++)
    {
        OLED_SetCursor(i, 0);
        for(j = 0; j < 128; j++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/* 初始化 OLED */
void OLED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* 1. 开 GPIOB 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* 2. PB6/PB7 配置为开漏输出 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 默认拉高 */
    OLED_W_SCL(1);
    OLED_W_SDA(1);

    /* 3. SSD1306 初始化 */
    OLED_WriteCommand(0xAE); // Display OFF

    OLED_WriteCommand(0xD5); // Set display clock divide ratio/oscillator frequency
    OLED_WriteCommand(0x80);

    OLED_WriteCommand(0xA8); // Set multiplex ratio
    OLED_WriteCommand(0x3F);

    OLED_WriteCommand(0xD3); // Set display offset
    OLED_WriteCommand(0x00);

    OLED_WriteCommand(0x40); // Set display start line

    OLED_WriteCommand(0xA1); // Segment remap
    OLED_WriteCommand(0xC8); // COM output scan direction

    OLED_WriteCommand(0xDA); // COM pins hardware configuration
    OLED_WriteCommand(0x12);

    OLED_WriteCommand(0x81); // Contrast control
    OLED_WriteCommand(0xCF);

    OLED_WriteCommand(0xD9); // Pre-charge period
    OLED_WriteCommand(0xF1);

    OLED_WriteCommand(0xDB); // VCOMH deselect level
    OLED_WriteCommand(0x30);

    OLED_WriteCommand(0xA4); // Entire display ON resume
    OLED_WriteCommand(0xA6); // Normal display

    OLED_WriteCommand(0x8D); // Charge pump setting
    OLED_WriteCommand(0x14);

    OLED_WriteCommand(0xAF); // Display ON

    OLED_Clear();
}

/* 普通 8x16 字符显示
   Line: 1~4
   Column: 1~16
*/
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t Index = Get_FontIndex(Char);
    uint8_t i;

    /* 上半部分 */
    OLED_SetCursor((Line - 1) * 2, (Column - 1) * 8);
    for(i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Index][i]);
    }

    /* 下半部分 */
    OLED_SetCursor((Line - 1) * 2 + 1, (Column - 1) * 8);
    for(i = 0; i < 8; i++)
    {
        OLED_WriteData(OLED_F8x16[Index][i + 8]);
    }
}

/* 普通字符串显示 */
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for(i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar(Line, Column + i, String[i]);
    }
}

/* 把 8 位纵向像素扩展为 16 位纵向像素
   每个点纵向复制 2 次
   例如 bit0 -> 2bit，bit1 -> 2bit ...
*/
static uint16_t OLED_ExpandByteTo16(uint8_t b)
{
    uint16_t result = 0;
    uint8_t i;

    for(i = 0; i < 8; i++)
    {
        if(b & (1 << i))
        {
            result |= (0x03 << (i * 2));
        }
    }

    return result;
}

/* 2倍放大字符显示：8x16 -> 16x32
   Line: 1~2
   Column: 1~8
*/
void OLED_ShowChar2x(uint8_t Line, uint8_t Column, char Char)
{
    uint8_t index = Get_FontIndex(Char);
    uint8_t i, j;
    uint16_t expanded;
    uint8_t startPage;
    uint8_t col;

    /* 放大后字符大小为 16x32
       高 32 像素 -> 占 4 页
       宽 16 像素 -> 占 16 列
    */
    startPage = (Line - 1) * 4;
    col = (Column - 1) * 16;

    /* 原字模共16列数据：
       前8字节是上半部（8列）
       后8字节是下半部（8列）
    */
    for(j = 0; j < 2; j++)   // j=0 上半，j=1 下半
    {
        for(i = 0; i < 8; i++)
        {
            expanded = OLED_ExpandByteTo16(OLED_F8x16[index][j * 8 + i]);

            /* expanded 是 16bit 高度，需要拆成两页 */
            /* 每一列横向也复制两次，实现 2 倍宽 */
            OLED_SetCursor(startPage + j * 2 + 0, col + i * 2);
            OLED_WriteData((uint8_t)(expanded & 0xFF));
            OLED_WriteData((uint8_t)(expanded & 0xFF));

            OLED_SetCursor(startPage + j * 2 + 1, col + i * 2);
            OLED_WriteData((uint8_t)((expanded >> 8) & 0xFF));
            OLED_WriteData((uint8_t)((expanded >> 8) & 0xFF));
        }
    }
}

/* 2倍放大字符串显示
   Line: 1~2
   Column: 1~8
*/
void OLED_ShowString2x(uint8_t Line, uint8_t Column, char *String)
{
    uint8_t i;
    for(i = 0; String[i] != '\0'; i++)
    {
        OLED_ShowChar2x(Line, Column + i, String[i]);
    }
}

/* ================== 新增整数显示函数 ================== */

/* 辅助函数：将 int32_t 转换为十进制字符串（不含正号，负号带 '-'） */
static void OLED_IntToStr(int32_t num, char *str)
{
    uint32_t temp;
    char *p = str;
    char buf[12];         // 足够容纳 -2147483648 及结束符
    uint8_t i;
	//volatile uint8_t len;

    if (num < 0)
    {
        *p++ = '-';
        temp = (uint32_t)(-num);
    }
    else
    {
        temp = (uint32_t)num;
    }

    /* 生成逆序数字 */
    i = 0;
    do {
        buf[i++] = (char)(temp % 10) + '0';
        temp /= 10;
    } while (temp > 0);

    //len = i;
    while (i > 0)
    {
        *p++ = buf[--i];
    }
    *p = '\0';
}

/* 在 (Line, Column) 显示整数（普通大小 8x16） */
void OLED_ShowInt(uint8_t Line, uint8_t Column, int32_t num)
{
    char str[13];   // 最大长度：负号+10位数字+结束符
    OLED_IntToStr(num, str);
    OLED_ShowString(Line, Column, str);
}

/* 2 倍放大版本（16x32） */
void OLED_ShowInt2x(uint8_t Line, uint8_t Column, int32_t num)
{
    char str[13];
    OLED_IntToStr(num, str);
    OLED_ShowString2x(Line, Column, str);
}
