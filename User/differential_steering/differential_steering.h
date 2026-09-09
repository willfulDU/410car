#ifndef __DIFFERENTIAL_STEERING_H
#define __DIFFERENTIAL_STEERING_H

#include <stdint.h>

#define DIFFERENTIAL_MAX_STEERING_DEG          30U
#define DIFFERENTIAL_STEERING_ADC_CENTER       0x078DU
#define DIFFERENTIAL_STEERING_ADC_LEFT_LIMIT   0U
#define DIFFERENTIAL_STEERING_ADC_RIGHT_LIMIT  4095U
#define DIFFERENTIAL_FULL_THROTTLE_RPM         300U
#define DIFFERENTIAL_CONTROL_PERIOD_MS          10U
#define DIFFERENTIAL_TARGET_RAMP_RPM_PER_STEP  8U
#define DIFFERENTIAL_SWAP_LEFT_RIGHT           0U

typedef struct
{
    uint16_t left_target_rpm;
    uint16_t right_target_rpm;
    uint8_t left_duty;
    uint8_t right_duty;
} DifferentialSteeringCommand;

void DifferentialSteering_Init(void);
void DifferentialSteering_Reset(void);
uint16_t DifferentialSteering_GetDifferencePermille(uint8_t degrees);
int8_t DifferentialSteering_MapSteeringDegrees(uint16_t wheel_adc);
uint16_t DifferentialSteering_MapCenterRpm(uint8_t pedal_percent);
void DifferentialSteering_BuildTargets(uint16_t center_rpm,
                                       int8_t steering_degrees,
                                       DifferentialSteeringCommand *command);
void DifferentialSteering_UpdateTargets(uint16_t left_target_rpm,
                                        uint16_t right_target_rpm,
                                        uint16_t left_measured_rpm,
                                        uint16_t right_measured_rpm,
                                        DifferentialSteeringCommand *command);

#endif
