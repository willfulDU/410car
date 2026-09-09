#ifndef TEST_STM32_H
#define TEST_STM32_H
#include <stdint.h>
typedef struct { volatile uint32_t SR, DR; } USART_TypeDef;
extern USART_TypeDef test_usart;
#define USART1 (&test_usart)
#define USART1_IRQn 37
#define RCC_APB2Periph_USART1 1
#define RCC_APB2Periph_GPIOA 2
#define GPIOA 0
#define GPIOB 1
#define RCC_APB2Periph_GPIOB 4
#define GPIO_Pin_6 64
#define GPIO_Pin_7 128
#define GPIO_Mode_Out_OD 3
#define Bit_SET 1
#define Bit_RESET 0
#define GPIO_Pin_9 9
#define GPIO_Pin_10 10
#define GPIO_Mode_AF_PP 1
#define GPIO_Mode_IN_FLOATING 2
#define GPIO_Speed_50MHz 50
#define USART_WordLength_8b 8
#define USART_StopBits_1 1
#define USART_Parity_No 0
#define USART_HardwareFlowControl_None 0
#define USART_Mode_Rx 1
#define USART_Mode_Tx 2
#define USART_IT_RXNE 32
#define USART_FLAG_RXNE 32
#define USART_FLAG_TXE 128
#define USART_SR_PE 1U
#define USART_SR_FE 2U
#define USART_SR_NE 4U
#define USART_SR_ORE 8U
#define USART_SR_RXNE 32U
#define RESET 0
#define ENABLE 1
typedef struct { uint32_t GPIO_Pin, GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
typedef struct { uint32_t USART_BaudRate, USART_WordLength, USART_StopBits, USART_Parity,
    USART_HardwareFlowControl, USART_Mode; } USART_InitTypeDef;
void RCC_APB2PeriphClockCmd(int clock, int enabled);
void GPIO_Init(int port, GPIO_InitTypeDef *init);
void GPIO_WriteBit(int port, uint16_t pin, int value);
void USART_Init(USART_TypeDef *usart, USART_InitTypeDef *init);
void USART_Cmd(USART_TypeDef *usart, int enabled);
void USART_SendData(USART_TypeDef *usart, uint16_t data);
int USART_GetFlagStatus(USART_TypeDef *usart, int flag);
uint16_t USART_ReceiveData(USART_TypeDef *usart);
void USART_ITConfig(USART_TypeDef *usart, int interrupt, int enabled);
void NVIC_SetPriority(int irq, uint32_t priority);
void NVIC_ClearPendingIRQ(int irq);
void NVIC_EnableIRQ(int irq);
uint32_t __get_PRIMASK(void);
void __disable_irq(void);
void __enable_irq(void);
#endif
