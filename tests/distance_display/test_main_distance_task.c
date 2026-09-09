#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "distance_display.h"

static unsigned pop_calls;
uint32_t SysTick_GetTick(void) { return 1000U; }
uint8_t USART_DistanceRx_Pop(uint8_t *byte, uint32_t *received_ms)
{
    ++pop_calls;
    *byte = '1';
    *received_ms = 1000U;
    /* Simulate a producer that keeps replenishing the queue. */
    return (uint8_t)(pop_calls <= 1000U);
}
void OLED_ShowString(uint8_t line, uint8_t column, char *text)
{
    (void)line; (void)column; (void)text;
}

/* Generated verbatim from the main ECU helper, without the MCU main(). */
#include "main_distance_task.inc"

int main(void)
{
    DistanceDisplay_Init(&s_distance_display);
    DistanceDisplay_Update();
    printf("Distance task consumed at most %u queue entries this iteration\n", pop_calls);
    if (pop_calls > 32U) {
        puts("FAIL: a busy UART delays the next radio/control iteration without a work limit");
        return 1;
    }
    puts("PASS: distance task yields to radio/control with a busy producer");
    return 0;
}
