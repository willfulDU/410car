#include "turn.h"

static uint16_t g_currentPulse = 1500;

/**********************************************************
 * Turn_Init
 * 功能：
 *   PB8 -> TIM4_CH3
 *   输出 50Hz 舵机 PWM
 *   分辨率 1us
 *
 * PWM 周期：
 *   20ms = 20000us
 *
 * 舵机脉宽：
 *   1000us -> 最小角度
 *   1500us -> 中位
 *   2000us -> 最大角度
 **********************************************************/
void Turn_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /* GPIOB、TIM4 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);

    /* PB8 -> TIM4_CH3 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /*
     * TIM4 时钟 72MHz
     * 72MHz / (71 + 1) = 1MHz
     * 1 计数 = 1us
     * 20ms 周期 -> ARR = 19999
     * PWM 频率 = 50Hz
     */
    TIM_TimeBaseStructure.TIM_Prescaler = 71;
    TIM_TimeBaseStructure.TIM_Period = 19999;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    /* TIM4_CH3 PWM 输出配置 */
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 1500;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC3Init(TIM4, &TIM_OCInitStructure);
    TIM_OC3PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM4, ENABLE);

    TIM_SetCompare3(TIM4, 1500);
    TIM_Cmd(TIM4, ENABLE);

    g_currentPulse = 1500;
}

/**********************************************************
 * Turn_SetPulse
 * 功能：直接设置舵机脉宽，单位 us
 *
 * pulse:
 *   1000 -> 最小角度
 *   1500 -> 中位
 *   2000 -> 最大角度
 **********************************************************/
void Turn_SetPulse(uint16_t pulse)
{
    if (pulse < 1000)
    {
        pulse = 1000;
    }

    if (pulse > 2000)
    {
        pulse = 2000;
    }

    g_currentPulse = pulse;
    TIM_SetCompare3(TIM4, pulse);
}

/**********************************************************
 * Turn_SetDuty
 * 功能：
 *   duty 0~100 映射为 1000~2000us
 *
 * duty:
 *   0   -> 1000us
 *   50  -> 1500us
 *   100 -> 2000us
 **********************************************************/
void Turn_SetDuty(uint8_t duty)
{
    if (duty > 100)
    {
        duty = 100;
    }

    g_currentPulse = 1000 + ((uint32_t)duty * 1000) / 100;
    TIM_SetCompare3(TIM4, g_currentPulse);
}

/**********************************************************
 * map_wheel
 * 功能：方向盘 ADC 值映射为舵机控制 duty
 *
 * 返回值范围大约：
 *   25 ~ 50 ~ 85
 **********************************************************/
uint8_t map_wheel(uint16_t pot)
{
    const uint16_t mid = 0x078D;

    if (pot <= mid)
    {
        return 25 + ((uint32_t)pot * 25) / mid;
    }
    else
    {
        return 50 + ((uint32_t)(pot - mid) * 35) / (4095 - mid);
    }
}
