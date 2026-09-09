#include "differential_steering.h"

#define DIFFERENTIAL_TABLE_COUNT       (DIFFERENTIAL_MAX_STEERING_DEG + 1U)
#define DIFFERENTIAL_PI_KP_NUM         20L
#define DIFFERENTIAL_PI_KI_NUM         2L
#define DIFFERENTIAL_PI_SCALE          100L
#define DIFFERENTIAL_PI_INTEGRAL_MAX   5000L
#define DIFFERENTIAL_DUTY_MAX          100U

static const uint16_t s_difference_permille[DIFFERENTIAL_TABLE_COUNT] =
{
    0U, 9U, 18U, 27U, 36U, 45U, 53U, 62U, 72U, 81U,
    90U, 99U, 108U, 117U, 127U, 136U, 146U, 156U, 165U,
    175U, 185U, 195U, 206U, 216U, 227U, 237U, 248U, 259U,
    271U, 282U, 294U
};

static uint16_t s_left_applied_target_rpm;
static uint16_t s_right_applied_target_rpm;
static int32_t s_left_integral;
static int32_t s_right_integral;

static uint16_t DifferentialSteering_RampTarget(uint16_t current,
                                                 uint16_t target)
{
    if (target > current)
    {
        if ((uint16_t)(target - current) > DIFFERENTIAL_TARGET_RAMP_RPM_PER_STEP)
        {
            return (uint16_t)(current + DIFFERENTIAL_TARGET_RAMP_RPM_PER_STEP);
        }
    }
    else if (current > target)
    {
        if ((uint16_t)(current - target) > DIFFERENTIAL_TARGET_RAMP_RPM_PER_STEP)
        {
            return (uint16_t)(current - DIFFERENTIAL_TARGET_RAMP_RPM_PER_STEP);
        }
    }

    return target;
}

static uint8_t DifferentialSteering_UpdatePi(uint16_t target_rpm,
                                              uint16_t measured_rpm,
                                              int32_t *integral)
{
    int32_t error;
    int32_t output;

    if (target_rpm == 0U)
    {
        *integral = 0L;
        return 0U;
    }

    error = (int32_t)target_rpm - (int32_t)measured_rpm;
    *integral += error;
    if (*integral > DIFFERENTIAL_PI_INTEGRAL_MAX)
    {
        *integral = DIFFERENTIAL_PI_INTEGRAL_MAX;
    }
    else if (*integral < -DIFFERENTIAL_PI_INTEGRAL_MAX)
    {
        *integral = -DIFFERENTIAL_PI_INTEGRAL_MAX;
    }

    output = (DIFFERENTIAL_PI_KP_NUM * error +
              DIFFERENTIAL_PI_KI_NUM * (*integral)) /
             DIFFERENTIAL_PI_SCALE;
    if (output <= 0L)
    {
        return 0U;
    }
    if (output >= (int32_t)DIFFERENTIAL_DUTY_MAX)
    {
        return DIFFERENTIAL_DUTY_MAX;
    }

    return (uint8_t)output;
}

void DifferentialSteering_Init(void)
{
    DifferentialSteering_Reset();
}

void DifferentialSteering_Reset(void)
{
    s_left_applied_target_rpm = 0U;
    s_right_applied_target_rpm = 0U;
    s_left_integral = 0L;
    s_right_integral = 0L;
}

uint16_t DifferentialSteering_GetDifferencePermille(uint8_t degrees)
{
    if (degrees > DIFFERENTIAL_MAX_STEERING_DEG)
    {
        degrees = DIFFERENTIAL_MAX_STEERING_DEG;
    }

    return s_difference_permille[degrees];
}

int8_t DifferentialSteering_MapSteeringDegrees(uint16_t wheel_adc)
{
    uint32_t numerator;
    uint16_t span;

    if (wheel_adc <= DIFFERENTIAL_STEERING_ADC_CENTER)
    {
        span = DIFFERENTIAL_STEERING_ADC_CENTER -
               DIFFERENTIAL_STEERING_ADC_LEFT_LIMIT;
        numerator = ((uint32_t)(DIFFERENTIAL_STEERING_ADC_CENTER - wheel_adc) *
                     DIFFERENTIAL_MAX_STEERING_DEG) + (span / 2U);
        return (int8_t)(numerator / span);
    }

    if (wheel_adc >= DIFFERENTIAL_STEERING_ADC_RIGHT_LIMIT)
    {
        return -(int8_t)DIFFERENTIAL_MAX_STEERING_DEG;
    }

    span = DIFFERENTIAL_STEERING_ADC_RIGHT_LIMIT -
           DIFFERENTIAL_STEERING_ADC_CENTER;
    numerator = ((uint32_t)(wheel_adc - DIFFERENTIAL_STEERING_ADC_CENTER) *
                 DIFFERENTIAL_MAX_STEERING_DEG) + (span / 2U);
    return -(int8_t)(numerator / span);
}

uint16_t DifferentialSteering_MapCenterRpm(uint8_t pedal_percent)
{
    if (pedal_percent > 100U)
    {
        pedal_percent = 100U;
    }

    return (uint16_t)(((uint32_t)pedal_percent *
                       DIFFERENTIAL_FULL_THROTTLE_RPM + 50U) / 100U);
}

void DifferentialSteering_BuildTargets(uint16_t center_rpm,
                                       int8_t steering_degrees,
                                       DifferentialSteeringCommand *command)
{
    uint8_t magnitude;
    uint16_t difference;
    uint16_t inner_rpm;
    uint16_t outer_rpm;
    int16_t steering;

    if (command == 0)
    {
        return;
    }

    steering = (int16_t)steering_degrees;
    if (steering < 0)
    {
        magnitude = (uint8_t)(-steering);
    }
    else
    {
        magnitude = (uint8_t)steering;
    }

    difference = (uint16_t)(((uint32_t)center_rpm *
                             DifferentialSteering_GetDifferencePermille(magnitude) +
                             500U) / 1000U);
    inner_rpm = (uint16_t)(center_rpm - (difference / 2U));
    outer_rpm = (uint16_t)(center_rpm + difference - (difference / 2U));

    if (steering >= 0)
    {
        command->left_target_rpm = inner_rpm;
        command->right_target_rpm = outer_rpm;
    }
    else
    {
        command->left_target_rpm = outer_rpm;
        command->right_target_rpm = inner_rpm;
    }

#if (DIFFERENTIAL_SWAP_LEFT_RIGHT != 0U)
    {
        uint16_t saved_left = command->left_target_rpm;
        command->left_target_rpm = command->right_target_rpm;
        command->right_target_rpm = saved_left;
    }
#endif

    command->left_duty = 0U;
    command->right_duty = 0U;
}

void DifferentialSteering_UpdateTargets(uint16_t left_target_rpm,
                                        uint16_t right_target_rpm,
                                        uint16_t left_measured_rpm,
                                        uint16_t right_measured_rpm,
                                        DifferentialSteeringCommand *command)
{
    if (command == 0)
    {
        return;
    }

    if ((left_target_rpm == 0U) && (right_target_rpm == 0U))
    {
        DifferentialSteering_Reset();
        command->left_target_rpm = 0U;
        command->right_target_rpm = 0U;
        command->left_duty = 0U;
        command->right_duty = 0U;
        return;
    }

    s_left_applied_target_rpm = DifferentialSteering_RampTarget(
        s_left_applied_target_rpm, left_target_rpm);
    s_right_applied_target_rpm = DifferentialSteering_RampTarget(
        s_right_applied_target_rpm, right_target_rpm);

    command->left_target_rpm = s_left_applied_target_rpm;
    command->right_target_rpm = s_right_applied_target_rpm;
    command->left_duty = DifferentialSteering_UpdatePi(
        s_left_applied_target_rpm, left_measured_rpm, &s_left_integral);
    command->right_duty = DifferentialSteering_UpdatePi(
        s_right_applied_target_rpm, right_measured_rpm, &s_right_integral);
}
