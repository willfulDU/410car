#ifndef __WS2812B_H
#define __WS2812B_H

#include "stm32f10x.h"

#define LED_NUM 5

void WS2812_Init(void);
void WS2812_SetPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
void WS2812_Show(void);
void WS2812_Clear(void);
void WS2812_Blink(uint8_t left, uint8_t right,
                  uint8_t r, uint8_t g, uint8_t b,
                  uint16_t delay);


#endif
