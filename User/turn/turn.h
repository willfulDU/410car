#ifndef __TURN_H
#define __TURN_H

#include "stm32f10x.h"

void Turn_Init(void);
void Turn_SetPulse(uint16_t pulse);
void Turn_SetDuty(uint8_t duty);
uint8_t map_wheel(uint16_t pot);

#endif
