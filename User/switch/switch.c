#include "switch.h"

/*
    引脚定义：

    R/D：
        PB5   -> R 信号
        PB7   -> D 信号

    左右：
        PB6   -> Left 信号
        PB8   -> Right 信号
*/

void Switch_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    /* PB5 -> R */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_5;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB7 -> D */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB6 -> Left */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    /* PB8 -> Right */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_8;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
}



DNR_State Get_DNR(void)
{
    uint8_t pb5_state;
    uint8_t pb7_state;

    pb5_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_5);
    pb7_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_7);

    /* PB5高电平，PB7低电平：R */
    if ((pb5_state == Bit_SET) && (pb7_state == Bit_RESET))
    {
        return DNR_REVERSE;
    }
    /* PB5低电平，PB7高电平：D */
    else if ((pb5_state == Bit_RESET) && (pb7_state == Bit_SET))
    {
        return DNR_FORWARD;
    }
    /* 其他情况：空档 */
    else
    {
        return DNR_NEUTRAL;
    }
}


Light_State Get_LeftRight(void)
{
    uint8_t pb6_state;
    uint8_t pb8_state;

    pb6_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_6);
    pb8_state = GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_8);

    /* PB6高电平，PB8低电平：Left */
    if ((pb6_state == Bit_SET) && (pb8_state == Bit_RESET))
    {
        return LIGHT_LEFT;
    }
    /* PB6低电平，PB8高电平：Right */
    else if ((pb6_state == Bit_RESET) && (pb8_state == Bit_SET))
    {
        return LIGHT_RIGHT;
    }
    /* 其他情况：无灯 */
    else
    {
        return LIGHT_NONE;
    }
}
