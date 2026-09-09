#include "distance_display.h"
#include <string.h>

void DistanceDisplay_Init(DistanceDisplay *state)
{
    memset(state, 0, sizeof(*state));
    state->invalid_line = 1U;
}

void DistanceDisplay_Receive(DistanceDisplay *state, uint8_t byte,
                             uint32_t received_ms)
{
    /* Never join a partial line to digits received after a disconnect. */
    if ((state->have_byte != 0U) &&
        ((uint32_t)(received_ms - state->last_byte_ms) >=
         DISTANCE_DISPLAY_TIMEOUT_MS))
    {
        state->value = 0U;
        state->digits = 0U;
        state->invalid_line = 1U;
    }
    state->last_byte_ms = received_ms;
    state->have_byte = 1U;

    if (byte == 0xFFU)
    {
        state->valid = 0U;
        state->value = 0U;
        state->digits = 0U;
        state->invalid_line = 1U;
    }
    else if ((byte == '\r') || (byte == '\n'))
    {
        if ((state->digits != 0U) && (state->invalid_line == 0U) &&
            (state->value <= 65535U))
        {
            state->distance_mm = (uint16_t)state->value;
            state->last_sample_ms = received_ms;
            /* Existing ranging firmware returns zero on sensor API failure. */
            state->valid = (uint8_t)(state->distance_mm != 0U);
        }
        state->value = 0U;
        state->digits = 0U;
        state->invalid_line = 0U;
    }
    else if ((byte >= '0') && (byte <= '9') &&
             (state->invalid_line == 0U) && (state->digits < 5U))
    {
        state->value = state->value * 10U + (uint32_t)(byte - '0');
        state->digits++;
    }
    else
    {
        state->invalid_line = 1U;
    }
}

void DistanceDisplay_Format(const DistanceDisplay *state, uint32_t now_ms,
                            char text[DISTANCE_DISPLAY_TEXT_SIZE])
{
    uint16_t value;
    uint8_t column;

    memcpy(text, "D:-----mm", DISTANCE_DISPLAY_TEXT_SIZE);
    if ((state->valid == 0U) ||
        ((uint32_t)(now_ms - state->last_sample_ms) >=
         DISTANCE_DISPLAY_TIMEOUT_MS))
    {
        return;
    }

    memset(text + 2, ' ', 5U);
    value = state->distance_mm;
    column = 7U;
    do
    {
        text[--column] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while (value != 0U);
}
