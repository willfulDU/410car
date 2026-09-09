#include <stdio.h>
#include <string.h>
#include "distance_display.h"
#include "usart/bsp_usart.h"

static unsigned checks;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
    printf("FAIL line %d: %s\n", __LINE__, #condition); return 1; } } while (0)

USART_TypeDef test_usart;
static uint32_t tick, primask;
void USART1_IRQHandler(void);
uint32_t SysTick_GetTick(void) { return tick; }
uint32_t __get_PRIMASK(void) { return primask; }
void __disable_irq(void) { primask = 1U; }
void __enable_irq(void) { primask = 0U; }
void RCC_APB2PeriphClockCmd(int clock, int enabled) { (void)clock; (void)enabled; }
void GPIO_Init(int port, GPIO_InitTypeDef *init) { (void)port; (void)init; }
void USART_Init(USART_TypeDef *u, USART_InitTypeDef *i) { (void)u; (void)i; }
void USART_Cmd(USART_TypeDef *u, int enabled) { (void)u; (void)enabled; }
void USART_SendData(USART_TypeDef *u, uint16_t d) { (void)u; (void)d; }
int USART_GetFlagStatus(USART_TypeDef *u, int f) { (void)u; (void)f; return 1; }
uint16_t USART_ReceiveData(USART_TypeDef *u) { return (uint16_t)u->DR; }
void USART_ITConfig(USART_TypeDef *u, int i, int e) { (void)u; (void)i; (void)e; }
void NVIC_SetPriority(int i, uint32_t p) { (void)i; (void)p; }
void NVIC_ClearPendingIRQ(int i) { (void)i; }
void NVIC_EnableIRQ(int i) { (void)i; }

static void feed(DistanceDisplay *state, const char *text, uint32_t at)
{
    while (*text) { DistanceDisplay_Receive(state, (uint8_t)*text++, at); }
}

static int shows(const DistanceDisplay *state, uint32_t at, const char *expected)
{
    char text[DISTANCE_DISPLAY_TEXT_SIZE];
    DistanceDisplay_Format(state, at, text);
    if (strcmp(text, expected) != 0) {
        printf("Expected [%s], got [%s]\n", expected, text);
        return 0;
    }
    return 1;
}

static void irq_byte(uint8_t byte, uint32_t at, uint32_t flags)
{
    test_usart.SR = flags;
    test_usart.DR = byte;
    tick = at;
    USART1_IRQHandler();
}

static void drain(DistanceDisplay *state)
{
    uint8_t byte;
    uint32_t at;
    while (USART_DistanceRx_Pop(&byte, &at)) { DistanceDisplay_Receive(state, byte, at); }
}

int main(void)
{
    DistanceDisplay state;
    uint8_t byte;
    uint32_t at;
    unsigned i;
    DistanceDisplay_Init(&state);
    CHECK(shows(&state, 0U, "D:NO RX  "));
    feed(&state, "56\n", 1U); /* attachment in the middle of a line */
    CHECK(shows(&state, 1U, "D:WAIT   "));
    feed(&state, "356\n", 10U);
    CHECK(shows(&state, 10U, "D:  356mm"));
    feed(&state, "42\r\n", 20U);
    CHECK(shows(&state, 20U, "D:   42mm"));
    feed(&state, "65535\n", 30U);
    CHECK(shows(&state, 30U, "D:65535mm"));
    feed(&state, "7", 40U);
    CHECK(shows(&state, 40U, "D:65535mm"));
    feed(&state, "89\n", 45U);
    CHECK(shows(&state, 45U, "D:  789mm"));
    feed(&state, "VL53L0X init complete\r\nAPI Status: 5\n\n", 50U);
    feed(&state, "65536\n123456\n000007\n-123\n12x34\n 55\n", 60U);
    CHECK(shows(&state, 60U, "D:  789mm"));
    CHECK(shows(&state, 544U, "D:  789mm"));
    CHECK(shows(&state, 545U, "D:FORMAT "));
    feed(&state, "\n789\n", 600U);
    CHECK(shows(&state, 600U, "D:  789mm")); /* same value must recover */
    feed(&state, "0\n", 610U);
    CHECK(shows(&state, 610U, "D:ZERO   "));
    feed(&state, "00001\n", 620U);
    CHECK(shows(&state, 620U, "D:    1mm"));
    feed(&state, "12", 630U);
    feed(&state, "3\n", 1130U); /* never join digits across a disconnect */
    CHECK(shows(&state, 1130U, "D:FORMAT "));
    feed(&state, "300\n", 1140U);
    CHECK(shows(&state, 1140U, "D:  300mm"));
    DistanceDisplay_Receive(&state, 0xFFU, 1150U);
    feed(&state, "45\n", 1150U);
    CHECK(shows(&state, 1150U, "D:RX ERR "));
    feed(&state, "123\n", 1160U);
    CHECK(shows(&state, 1160U, "D:  123mm"));
    DistanceDisplay_Init(&state);
    feed(&state, "\n250\n", UINT32_MAX - 100U);
    CHECK(shows(&state, 398U, "D:  250mm"));
    CHECK(shows(&state, 399U, "D:STALE  "));

    USART_Config();
    USART_DistanceRx_Enable();
    CHECK(!USART_DistanceRx_Pop(&byte, &at));
    for (i = 0U; i < 400U; ++i) { /* wrap the FIFO repeatedly */
        irq_byte((uint8_t)(i % 255U), i, USART_SR_RXNE);
        CHECK(USART_DistanceRx_Pop(&byte, &at));
        CHECK(byte == (uint8_t)(i % 255U) && at == i);
    }
    primask = 1U;
    CHECK(!USART_DistanceRx_Pop(&byte, &at));
    CHECK(primask == 1U);
    primask = 0U;
    for (i = 0U; i < 200U; ++i) { irq_byte('1', i, USART_SR_RXNE); }
    CHECK(USART_DistanceRx_Pop(&byte, &at));
    CHECK(byte == 0xFFU);
    CHECK(!USART_DistanceRx_Pop(&byte, &at));
    CHECK(primask == 0U);
    for (i = USART_SR_PE; i <= USART_SR_ORE; i <<= 1) {
        irq_byte('9', 1000U, i | USART_SR_RXNE);
        CHECK(USART_DistanceRx_Pop(&byte, &at));
        CHECK(byte == 0xFFU);
        CHECK(!USART_DistanceRx_Pop(&byte, &at));
    }
    irq_byte('9', 1000U, USART_SR_ORE); /* ORE without RXNE */
    CHECK(USART_DistanceRx_Pop(&byte, &at) && byte == 0xFFU);
    DistanceDisplay_Init(&state);
    irq_byte('\n', 1100U, USART_SR_RXNE);
    irq_byte('3', 1100U, USART_SR_RXNE);
    irq_byte('5', 1100U, USART_SR_RXNE);
    irq_byte('6', 1100U, USART_SR_RXNE);
    irq_byte('\n', 1100U, USART_SR_RXNE);
    tick = 1700U;
    drain(&state);
    CHECK(shows(&state, 1700U, "D:STALE  ")); /* dequeue does not refresh age */
    irq_byte('\n', 1800U, USART_SR_RXNE);
    irq_byte('4', 1800U, USART_SR_RXNE);
    irq_byte('2', 1800U, USART_SR_RXNE);
    irq_byte('\n', 1800U, USART_SR_RXNE);
    drain(&state);
    CHECK(shows(&state, 1800U, "D:   42mm"));
    /* A clean line boundary remains valid across a slow sampling interval. */
    DistanceDisplay_Init(&state);
    feed(&state, "\n356\n", 10U);
    feed(&state, "420\n", 1010U);
    CHECK(shows(&state, 1010U, "D:  420mm"));
    feed(&state, "500\n", 2010U);
    CHECK(shows(&state, 2010U, "D:  500mm"));
    CHECK(shows(&state, 2510U, "D:STALE  "));
    /* Unknown text is diagnosed; numbers are never harvested from debug text. */
    DistanceDisplay_Init(&state);
    feed(&state, "\nDistance: 123 mm\n", 1U);
    CHECK(shows(&state, 1U, "D:FORMAT "));
    feed(&state, "123", 2U);
    CHECK(shows(&state, 2U, "D:WAIT   "));
    feed(&state, "\n", 3U);
    CHECK(shows(&state, 3U, "D:  123mm"));
    feed(&state, "\n0\n", 1000U);
    CHECK(shows(&state, 1000U, "D:ZERO   "));
    feed(&state, "\n", 1400U);
    CHECK(shows(&state, 1500U, "D:STALE  ")); /* empty lines do not renew a zero sample */
    feed(&state, "123\n", 1510U);
    feed(&state, "\n", 1910U);
    CHECK(shows(&state, 2010U, "D:STALE  ")); /* nor a previous valid reading */
    puts("PASS: parser, display, freshness, FIFO and UART error behavior");
    printf("%u checks passed\n", checks);
    return 0;
}
