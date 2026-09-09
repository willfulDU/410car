#ifndef DISTANCE_DISPLAY_H
#define DISTANCE_DISPLAY_H

#include <stdint.h>

#define DISTANCE_DISPLAY_PERIOD_MS 200U
#define DISTANCE_DISPLAY_TIMEOUT_MS 500U
#define DISTANCE_DISPLAY_TEXT_SIZE 10U

typedef struct
{
    uint32_t value;
    uint32_t last_byte_ms;
    uint32_t last_sample_ms;
    uint16_t distance_mm;
    uint8_t digits;
    uint8_t invalid_line;
    uint8_t valid;
    uint8_t have_byte;
} DistanceDisplay;

/* Start at the next line boundary, including when attaching mid-stream. */
void DistanceDisplay_Init(DistanceDisplay *state);
/* 0xFF reports lost/corrupt UART bytes; discard through the next CR/LF. */
void DistanceDisplay_Receive(DistanceDisplay *state, uint8_t byte,
                             uint32_t received_ms);
/* Nine fixed-width characters plus NUL; stale/zero readings use dashes. */
void DistanceDisplay_Format(const DistanceDisplay *state, uint32_t now_ms,
                            char text[DISTANCE_DISPLAY_TEXT_SIZE]);

#endif
