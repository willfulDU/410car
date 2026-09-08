#ifndef __PEDAL_H
#define __PEDAL_H

#include "stm32f10x.h"

void Pedal_ADC_Init(void);
uint16_t Pedal_ADC_ReadRaw(void);
float Pedal_GetVoltage(void);
uint8_t Pedal_GetValue(void);
uint16_t Pedal_ADC_ReadAverage(uint8_t times);
uint8_t Pedal_GetValue255_Avg(uint8_t times);

#endif
