#include "wheel_speed.h"
#include "bsp_SysTick.h"
#include "misc.h"

#define WHEEL_SIDE_COUNT            2U
#define WHEEL_SPEED_WINDOW_CAP_MS   60000UL

#if (WHEEL_SPEED_SWAP_SIDES == 0U)
#define WHEEL_SIDE_OF_PA1   WHEEL_RIGHT
#define WHEEL_SIDE_OF_PA2   WHEEL_LEFT
#else
#define WHEEL_SIDE_OF_PA1   WHEEL_LEFT
#define WHEEL_SIDE_OF_PA2   WHEEL_RIGHT
#endif

static volatile uint32_t s_pulses[WHEEL_SIDE_COUNT];
static uint32_t s_last_pulses[WHEEL_SIDE_COUNT];
static uint16_t s_rpm[WHEEL_SIDE_COUNT];
static uint32_t s_window_start_ms;

void WheelSpeed_Init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;
    uint8_t i;

    for (i = 0U; i < WHEEL_SIDE_COUNT; ++i)
    {
        s_pulses[i] = 0U;
        s_last_pulses[i] = 0U;
        s_rpm[i] = 0U;
    }

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);

    gpio.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource1);
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOA, GPIO_PinSource2);

    exti.EXTI_Line = EXTI_Line1 | EXTI_Line2;
    exti.EXTI_Mode = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Rising;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);
    EXTI_ClearITPendingBit(EXTI_Line1 | EXTI_Line2);

    nvic.NVIC_IRQChannelPreemptionPriority = 0U;
    nvic.NVIC_IRQChannelSubPriority = 0U;
    nvic.NVIC_IRQChannelCmd = ENABLE;
    nvic.NVIC_IRQChannel = EXTI1_IRQn;
    NVIC_Init(&nvic);
    nvic.NVIC_IRQChannel = EXTI2_IRQn;
    NVIC_Init(&nvic);

    s_window_start_ms = SysTick_GetTick();
}

void WheelSpeed_Update(void)
{
    uint32_t now;
    uint32_t elapsed;
    uint32_t snapshot;
    uint32_t delta;
    uint8_t i;

    now = SysTick_GetTick();
    elapsed = now - s_window_start_ms;
    if (elapsed < WHEEL_SPEED_WINDOW_MS)
    {
        return;
    }

    s_window_start_ms = now;
    if (elapsed > WHEEL_SPEED_WINDOW_CAP_MS)
    {
        elapsed = WHEEL_SPEED_WINDOW_CAP_MS;
    }

    for (i = 0U; i < WHEEL_SIDE_COUNT; ++i)
    {
        snapshot = s_pulses[i];
        delta = snapshot - s_last_pulses[i];
        s_last_pulses[i] = snapshot;
        s_rpm[i] = WheelSpeedCalc_Filter(
            s_rpm[i],
            WheelSpeedCalc_Rpm(delta, (uint16_t)elapsed,
                               WHEEL_SPEED_PULSES_PER_REV));
    }
}

uint16_t WheelSpeed_GetRpm(WheelSide side)
{
    if ((uint8_t)side >= WHEEL_SIDE_COUNT)
    {
        return 0U;
    }

    return s_rpm[(uint8_t)side];
}

uint32_t WheelSpeed_GetPulses(WheelSide side)
{
    if ((uint8_t)side >= WHEEL_SIDE_COUNT)
    {
        return 0U;
    }

    return s_pulses[(uint8_t)side];
}

void EXTI1_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line1) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line1);
        s_pulses[WHEEL_SIDE_OF_PA1]++;
    }
}

void EXTI2_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line2) != RESET)
    {
        EXTI_ClearITPendingBit(EXTI_Line2);
        s_pulses[WHEEL_SIDE_OF_PA2]++;
    }
}
