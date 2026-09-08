#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"
#include <stdint.h>

/* 引脚定义：PB6 -> SCL, PB7 -> SDA */
#define OLED_SCL_PORT    GPIOB
#define OLED_SCL_PIN     GPIO_Pin_6
#define OLED_SDA_PORT    GPIOB
#define OLED_SDA_PIN     GPIO_Pin_7

/* 写引脚电平 */
#define OLED_W_SCL(x)    GPIO_WriteBit(OLED_SCL_PORT, OLED_SCL_PIN, (x) ? Bit_SET : Bit_RESET)
#define OLED_W_SDA(x)    GPIO_WriteBit(OLED_SDA_PORT, OLED_SDA_PIN, (x) ? Bit_SET : Bit_RESET)

/* 基本函数 */
void OLED_Init(void);
void OLED_Clear(void);

/* 普通 8x16 字符显示 */
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowInt(uint8_t Line, uint8_t Column, int32_t num);

/* 2倍放大字符显示：8x16 -> 16x32 */
void OLED_ShowChar2x(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString2x(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowInt2x(uint8_t Line, uint8_t Column, int32_t num);

#endif
