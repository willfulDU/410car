#include "motor.h"

#define MOTOR_PWM_ARR   4799

uint16_t motor2_ccr = 0;
uint16_t motor1_ccr = 0;

/**********************************************************
 * Motor_Init
 * PWM + IO 初始化
 * PA8  -> TIM1_CH1 PWM
 * PA11 -> TIM1_CH4 PWM
 * PWM频率 15kHz
 * PB4  -> 方向1
 * PB5  -> 方向2
 * PA2  -> Motor1测速输入
 * PA1  -> Motor2测速输入
 **********************************************************/
void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    /* GPIOA / GPIOB / AFIO / TIM1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_AFIO | RCC_APB2Periph_TIM1, ENABLE);

    /* 关闭JTAG，保留SWD，释放PB4 */
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);

    /******************** PA8 / PA11 -> TIM1 PWM ********************/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8 | GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /******************** PB4 / PB5 -> 方向控制 ********************/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4 | GPIO_Pin_5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* 默认停止 */
    GPIO_ResetBits(GPIOB, GPIO_Pin_4 | GPIO_Pin_5);

    /******************** TIM1 计时器 ********************/
    TIM_TimeBaseStructure.TIM_Period = MOTOR_PWM_ARR;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM1, &TIM_TimeBaseStructure);

    /******************** TIM1_CH1 PWM PA8 ********************/
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC1Init(TIM1, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);

    /******************** TIM1_CH4 PWM PA11 ********************/
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_OutputNState = TIM_OutputNState_Disable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OCNPolarity = TIM_OCNPolarity_High;
    TIM_OCInitStructure.TIM_OCIdleState = TIM_OCIdleState_Reset;
    TIM_OCInitStructure.TIM_OCNIdleState = TIM_OCNIdleState_Reset;
    TIM_OC4Init(TIM1, &TIM_OCInitStructure);
    TIM_OC4PreloadConfig(TIM1, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM1, ENABLE);

    /* 初始占空比 0% */
    TIM_SetCompare1(TIM1, MOTOR_PWM_ARR + 1);
    TIM_SetCompare4(TIM1, MOTOR_PWM_ARR + 1);
    motor1_ccr = MOTOR_PWM_ARR + 1;
    motor2_ccr = MOTOR_PWM_ARR + 1;

    /* 高级定时器使能输出 */
    TIM_CtrlPWMOutputs(TIM1, ENABLE);
    TIM_Cmd(TIM1, ENABLE);
}

/**********************************************************
 * Motor2_SetDuty
 * PA11 PWM 占空比
 * duty: 0 ~ 100
 **********************************************************/
void Motor2_SetDuty(uint8_t duty)
{
    if (duty > 100) duty = 100;
    motor2_ccr = ((uint32_t)(100 - duty) * (MOTOR_PWM_ARR + 1)) / 100;
    TIM_SetCompare4(TIM1, motor2_ccr);
}

/**********************************************************
 * Motor1_SetDuty
 * PA8 PWM 占空比
 * duty: 0 ~ 100
 **********************************************************/
void Motor1_SetDuty(uint8_t duty)
{
    if (duty > 100) duty = 100;
    motor1_ccr = ((uint32_t)(100 - duty) * (MOTOR_PWM_ARR + 1)) / 100;
    TIM_SetCompare1(TIM1, motor1_ccr);
}

/**********************************************************
 * Motor_SetPB4
 **********************************************************/
void Motor_SetPB4(uint8_t level)
{
    if (level) GPIO_SetBits(GPIOB, GPIO_Pin_4);
    else GPIO_ResetBits(GPIOB, GPIO_Pin_4);
}

/**********************************************************
 * Motor_SetPB5
 **********************************************************/
void Motor_SetPB5(uint8_t level)
{
    if (level) GPIO_SetBits(GPIOB, GPIO_Pin_5);
    else GPIO_ResetBits(GPIOB, GPIO_Pin_5);
}

/**********************************************************
 * Motor_SetCW
 * status:
 * 0 -> 停止
 * 1 -> 正转
 * 2 -> 反转
 **********************************************************/
void Motor_SetCW(uint8_t status)
{
    /* Disable PWM before changing the H-bridge direction inputs. */
    Motor1_SetDuty(0);
    Motor2_SetDuty(0);

    switch (status)
    {
        case 1: /* Forward */
            Motor_SetPB4(1);
            Motor_SetPB5(0);
            break;

        case 2: /* Reverse */
            Motor_SetPB4(0);
            Motor_SetPB5(1);
            break;

        case 0: /* Neutral / stop */
        default:
            Motor_SetPB4(0);
            Motor_SetPB5(0);
            break;
    }
}

/**********************************************************
 * map_pedal
 * ADC 值映射为 0~100 占空比
 **********************************************************/
uint8_t map_pedal(uint16_t pot)
{
    if (pot >= 0xC8) return 0;
    if (pot <= 0x78) return 100;
    return (uint8_t)(((uint32_t)(0xC8 - pot) * 100) / (0xC8 - 0x78));
}
