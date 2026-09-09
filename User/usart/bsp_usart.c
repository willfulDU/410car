/**
  ******************************************************************************
  * @file    bsp_usart.c
  * @version V1.0
  * @date    2013-xx-xx
  * @brief   调试用的printf串口，重定向printf到串口
  ******************************************************************************
  * @attention
  *
  * 实验平台:野火 F103 STM32 核心板 
  * 论坛    :http://www.firebbs.cn
  * 淘宝    :https://fire-stm32.taobao.com
  *
  ******************************************************************************
  */ 


#include "./usart/bsp_usart.h"
#include "main.h"

#if defined(_MAIN_ECU_) && !defined(__RF24L01_TX_TEST__)
#include "SysTick/bsp_SysTick.h"
#endif


 /**
  * @brief  USART GPIO 配置,工作参数配置
  * @param  无
  * @retval 无
  */
void USART_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;

	// 打开串口GPIO的时钟
	DEBUG_USART_GPIO_APBxClkCmd(DEBUG_USART_GPIO_CLK, ENABLE);
	
	// 打开串口外设的时钟
	DEBUG_USART_APBxClkCmd(DEBUG_USART_CLK, ENABLE);

	// 将USART Tx的GPIO配置为推挽复用模式
	GPIO_InitStructure.GPIO_Pin = DEBUG_USART_TX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(DEBUG_USART_TX_GPIO_PORT, &GPIO_InitStructure);

  // 将USART Rx的GPIO配置为浮空输入模式
	GPIO_InitStructure.GPIO_Pin = DEBUG_USART_RX_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(DEBUG_USART_RX_GPIO_PORT, &GPIO_InitStructure);
	
	// 配置串口的工作参数
	// 配置波特率
	USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;
	// 配置 针数据字长
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;
	// 配置停止位
	USART_InitStructure.USART_StopBits = USART_StopBits_1;
	// 配置校验位
	USART_InitStructure.USART_Parity = USART_Parity_No ;
	// 配置硬件流控制
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	// 配置工作模式，收发一起
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
	// 完成串口的初始化配置
	USART_Init(DEBUG_USARTx, &USART_InitStructure);

	// 使能串口
	USART_Cmd(DEBUG_USARTx, ENABLE);	    
}


///重定向c库函数printf到串口，重定向后可使用printf函数
int fputc(int ch, FILE *f)
{
		/* 发送一个字节数据到串口 */
		USART_SendData(DEBUG_USARTx, (uint8_t) ch);
		
		/* 等待发送完毕 */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TXE) == RESET);		
	
		return (ch);
}

///重定向c库函数scanf到串口，重写向后可使用scanf、getchar等函数
int fgetc(FILE *f)
{
		/* 等待串口输入数据 */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);

		return (int)USART_ReceiveData(DEBUG_USARTx);
}


#if defined(_MAIN_ECU_) && !defined(__RF24L01_TX_TEST__)

#define DISTANCE_RX_BUFFER_SIZE 128U

/* Each byte retains its arrival time; delayed consumption is not fresh data. */
typedef struct
{
    uint32_t received_ms;
    uint8_t data;
} DistanceRxByte;

static volatile DistanceRxByte s_distance_rx[DISTANCE_RX_BUFFER_SIZE];
static volatile uint16_t s_distance_head;
static volatile uint16_t s_distance_tail;
static volatile uint8_t s_distance_rx_error;

void USART_DistanceRx_Enable(void)
{
    uint32_t irq_mask = __get_PRIMASK();
    volatile uint32_t discarded;

    __disable_irq();
    s_distance_head = 0U;
    s_distance_tail = 0U;
    s_distance_rx_error = 0U;
    /* Reading SR followed by DR clears RXNE and any pre-existing errors. */
    discarded = DEBUG_USARTx->SR;
    discarded = DEBUG_USARTx->DR;
    (void)discarded;
    NVIC_ClearPendingIRQ(DEBUG_USART_IRQ);
    NVIC_SetPriority(DEBUG_USART_IRQ, 1U);
    NVIC_EnableIRQ(DEBUG_USART_IRQ);
    USART_ITConfig(DEBUG_USARTx, USART_IT_RXNE, ENABLE);
    if (irq_mask == 0U)
    {
        __enable_irq();
    }
}

uint8_t USART_DistanceRx_Pop(uint8_t *data, uint32_t *received_ms)
{
    uint32_t irq_mask;
    uint8_t available = 0U;
    uint16_t tail;

    if ((data == 0) || (received_ms == 0))
    {
        return 0U;
    }

    irq_mask = __get_PRIMASK();
    __disable_irq();
    if (s_distance_rx_error != 0U)
    {
        /* Drop the entire queue, never join digits across a lost byte. */
        s_distance_tail = s_distance_head;
        s_distance_rx_error = 0U;
        *data = 0xFFU;
        *received_ms = SysTick_GetTick();
        available = 1U;
    }
    else if (s_distance_tail != s_distance_head)
    {
        tail = s_distance_tail;
        *data = s_distance_rx[tail].data;
        *received_ms = s_distance_rx[tail].received_ms;
        s_distance_tail = (uint16_t)((tail + 1U) % DISTANCE_RX_BUFFER_SIZE);
        available = 1U;
    }
    if (irq_mask == 0U)
    {
        __enable_irq();
    }
    return available;
}

void DEBUG_USART_IRQHandler(void)
{
    uint32_t status = DEBUG_USARTx->SR;
    uint8_t data;
    uint16_t next_head;

    if ((status & (USART_SR_RXNE | USART_SR_ORE | USART_SR_FE |
                   USART_SR_NE | USART_SR_PE)) == 0U)
    {
        return;
    }

    /* Also clears errors after WS2812's interrupt-masked SPI transfers. */
    data = (uint8_t)DEBUG_USARTx->DR;
    if ((status & (USART_SR_ORE | USART_SR_FE | USART_SR_NE | USART_SR_PE)) != 0U)
    {
        s_distance_rx_error = 1U;
        return;
    }
    if (s_distance_rx_error != 0U)
    {
        return;
    }

    next_head = (uint16_t)((s_distance_head + 1U) % DISTANCE_RX_BUFFER_SIZE);
    if (next_head == s_distance_tail)
    {
        s_distance_rx_error = 1U;
        return;
    }
    s_distance_rx[s_distance_head].data = data;
    s_distance_rx[s_distance_head].received_ms = SysTick_GetTick();
    s_distance_head = next_head;
}

#endif
