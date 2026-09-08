#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f10x.h"

extern uint16_t motor2_ccr;
extern uint16_t motor1_ccr;

void Motor_Init(void);
void Motor2_SetDuty(uint8_t duty);
void Motor1_SetDuty(uint8_t duty);
void Motor_SetPB4(uint8_t level);
void Motor_SetPB5(uint8_t level);
void Motor_SetCW(uint8_t status);
uint8_t map_pedal(uint16_t pot);


#endif
