#include "wheel_speed_calc.h"

uint16_t WheelSpeedCalc_Rpm(uint32_t pulse_delta, uint16_t window_ms,
                            uint16_t pulses_per_rev)
{
    uint32_t divisor;
    uint64_t rpm;

    if ((window_ms == 0U) || (pulses_per_rev == 0U))
    {
        return 0U;
    }

    divisor = (uint32_t)window_ms * (uint32_t)pulses_per_rev;
    rpm = ((uint64_t)pulse_delta * 60000ULL + (uint64_t)(divisor / 2U)) /
          (uint64_t)divisor;

    if (rpm > (uint64_t)WHEEL_SPEED_RPM_MAX)
    {
        return WHEEL_SPEED_RPM_MAX;
    }

    return (uint16_t)rpm;
}

uint16_t WheelSpeedCalc_Filter(uint16_t previous, uint16_t sample)
{
    uint16_t difference;

    if (sample > previous)
    {
        difference = (uint16_t)(sample - previous);
        return (uint16_t)(previous + ((difference + 1U) / 2U));
    }

    difference = (uint16_t)(previous - sample);
    return (uint16_t)(previous - ((difference + 1U) / 2U));
}

void WheelSpeedCalc_Format(char out[WHEEL_SPEED_TEXT_LEN], uint16_t rpm)
{
    uint8_t i;

    if (rpm > WHEEL_SPEED_RPM_MAX)
    {
        out[0] = '-';
        out[1] = '-';
        out[2] = '-';
        out[3] = '-';
        out[4] = '\0';
        return;
    }

    out[4] = '\0';
    for (i = 4U; i > 0U; --i)
    {
        out[i - 1U] = (char)('0' + (rpm % 10U));
        rpm = (uint16_t)(rpm / 10U);
    }

    for (i = 0U; i < 3U; ++i)
    {
        if (out[i] != '0')
        {
            break;
        }
        out[i] = ' ';
    }
}
