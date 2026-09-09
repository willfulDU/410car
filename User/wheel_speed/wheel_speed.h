#ifndef __WHEEL_SPEED_H
#define __WHEEL_SPEED_H

#include "stm32f10x.h"
#include "wheel_speed_calc.h"

/* Reference-car calibration: motor FG pulses per rear-wheel revolution. */
#define WHEEL_SPEED_PULSES_PER_REV  398U
#define WHEEL_SPEED_WINDOW_MS        250U
#define WHEEL_SPEED_FEEDBACK_TIMEOUT_MS  750U
#define WHEEL_SPEED_SWAP_SIDES       1U
#define WHEEL_SPEED_CALIBRATE        0U

typedef enum
{
    WHEEL_LEFT = 0,
    WHEEL_RIGHT
} WheelSide;

void WheelSpeed_Init(void);
void WheelSpeed_Update(void);
uint16_t WheelSpeed_GetRpm(WheelSide side);
uint32_t WheelSpeed_GetPulses(WheelSide side);
uint8_t WheelSpeed_HasFreshFeedback(WheelSide side);

#endif
