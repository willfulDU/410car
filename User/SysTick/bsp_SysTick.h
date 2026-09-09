#ifndef __SYSTICK_H
#define __SYSTICK_H

#include "stm32f10x.h"


#define  delay_ms(x)  SysTick_Delay_Ms(x)
#define  delay_us(x)  SysTick_Delay_Us(x)


void SysTick_Init(void);
void Delay_us(__IO u32 nTime);
#define Delay_ms(x) Delay_us(100*x)	 //单位ms

void SysTick_Delay_Us( __IO uint32_t us);
void SysTick_Delay_Ms( __IO uint32_t ms);

void TimingDelay_Decrement(void);

/* 1s 时基：计时在 SysTick 中断里完成，main.c 只读/清标志位 */
void SysTick_Counter(void);     /* 在 SysTick_Handler 中调用，累计并置 1s 标志 */
uint32_t SysTick_GetFlag(void); /* 读取 1s 标志（1 = 已到 1 秒） */
void SysTick_ClearFlag(void);   /* 清除 1s 标志 */


#endif /* __SYSTICK_H */
