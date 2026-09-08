#ifndef __WHEEL_H
#define __WHEEL_H

#include "stm32f10x.h"

void Wheel_ADC_Init(void);
uint16_t Wheel_GetValue(void);

#endif
