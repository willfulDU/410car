#ifndef __WHEEL_SPEED_CALC_H
#define __WHEEL_SPEED_CALC_H

#include <stdint.h>

#define WHEEL_SPEED_RPM_MAX   9999U
#define WHEEL_SPEED_TEXT_LEN  5U

uint16_t WheelSpeedCalc_Rpm(uint32_t pulse_delta, uint16_t window_ms,
                            uint16_t pulses_per_rev);
uint16_t WheelSpeedCalc_Filter(uint16_t previous, uint16_t sample);
void WheelSpeedCalc_Format(char out[WHEEL_SPEED_TEXT_LEN], uint16_t rpm);

#endif
