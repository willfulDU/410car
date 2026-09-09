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
    /* Discard interrupted partial lines, but accept a new complete line even
       if the sensor sends less often than the display's freshness timeout. */
    if ((state->have_byte != 0U) &&
        ((state->digits != 0U) || (state->invalid_line != 0U)) &&
        ((uint32_t)(received_ms - state->last_byte_ms) >=
         DISTANCE_DISPLAY_TIMEOUT_MS))
    {
        state->value = 0U;
        state->digits = 0U;
        state->invalid_line = 1U;
        if (state->status != DISTANCE_STATUS_UART_ERROR)
        {
            state->status = DISTANCE_STATUS_FORMAT_ERROR;
        }
    }
    state->last_byte_ms = received_ms;
    state->have_byte = 1U;

    if (byte == 0xFFU)
    {
        state->status = DISTANCE_STATUS_UART_ERROR;
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
            state->status = (state->valid != 0U) ?
                DISTANCE_STATUS_OK : DISTANCE_STATUS_ZERO;
        }
        else if ((state->digits != 0U) && (state->value > 65535U))
        {
            state->status = DISTANCE_STATUS_FORMAT_ERROR;
        }
        state->value = 0U;
        state->digits = 0U;
        state->invalid_line = 0U;
    }
    else if ((byte >= '0') && (byte <= '9') &&
             (state->invalid_line == 0U) && (state->digits < 5U))
    {
        if (state->digits == 0U)
        {
            state->status = DISTANCE_STATUS_WAITING;
        }
        state->value = state->value * 10U + (uint32_t)(byte - '0');
        state->digits++;
    }
    else
    {
        state->invalid_line = 1U;
        if ((state->status != DISTANCE_STATUS_UART_ERROR) &&
            ((byte < '0') || (byte > '9') || (state->digits >= 5U)))
        {
            state->status = DISTANCE_STATUS_FORMAT_ERROR;
        }
    }
}

void DistanceDisplay_Format(const DistanceDisplay *state, uint32_t now_ms,
                            char text[DISTANCE_DISPLAY_TEXT_SIZE])
{
    uint16_t value;
    uint8_t column;

    memcpy(text, "D:NO RX  ", DISTANCE_DISPLAY_TEXT_SIZE);
    if (state->have_byte == 0U)
    {
        return;
    }
    if ((uint32_t)(now_ms - state->last_byte_ms) >= DISTANCE_DISPLAY_TIMEOUT_MS)
    {
        memcpy(text, "D:STALE  ", DISTANCE_DISPLAY_TEXT_SIZE);
        return;
    }
    if ((state->valid == 0U) ||
        ((uint32_t)(now_ms - state->last_sample_ms) >=
         DISTANCE_DISPLAY_TIMEOUT_MS))
    {
        switch (state->status)
        {
        case DISTANCE_STATUS_OK:
            memcpy(text, "D:STALE  ", DISTANCE_DISPLAY_TEXT_SIZE);
            break;
        case DISTANCE_STATUS_ZERO:
            if ((uint32_t)(now_ms - state->last_sample_ms) >= DISTANCE_DISPLAY_TIMEOUT_MS)
            {
                memcpy(text, "D:STALE  ", DISTANCE_DISPLAY_TEXT_SIZE);
            }
            else
            {
                memcpy(text, "D:ZERO   ", DISTANCE_DISPLAY_TEXT_SIZE);
            }
            break;
        case DISTANCE_STATUS_FORMAT_ERROR:
            memcpy(text, "D:FORMAT ", DISTANCE_DISPLAY_TEXT_SIZE);
            break;
        case DISTANCE_STATUS_UART_ERROR:
            memcpy(text, "D:RX ERR ", DISTANCE_DISPLAY_TEXT_SIZE);
            break;
        default:
            memcpy(text, "D:WAIT   ", DISTANCE_DISPLAY_TEXT_SIZE);
            break;
        }
        return;
    }

    memcpy(text, "D:     mm", DISTANCE_DISPLAY_TEXT_SIZE);
    memset(text + 2, ' ', 5U);
    value = state->distance_mm;
    column = 7U;
    do
    {
        text[--column] = (char)('0' + (value % 10U));
        value = (uint16_t)(value / 10U);
    } while (value != 0U);
}
