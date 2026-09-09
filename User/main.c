#include "stm32f10x.h"
#include "main.h"
#include "usart/bsp_usart.h"
#include "SysTick/bsp_SysTick.h"
#include <stdio.h>
#include <string.h>

#ifdef __RF24L01_TX_TEST__   //发射端头文件
#include "drv_RF24L01.h"
#include "drv_delay.h"
#include "drv_spi.h"
#include "wheel.h"
#include "pedal.h"
#include "switch.h"

#else                        //接收端头文件
#include "ws2812b.h"
#include "vl53l0x.h"
#include "motor.h"
#include "turn.h"
#include "oled.h"

#endif

#define TX_DATA_LEN  8   // 每帧发送 8 个字节（即 8 个两位十六进制数）

/* 全局变量（如需从串口接收数据可启用） */
uint8_t g_UartRxBuffer[100] = {0};

/* 发送端：发送数据 */
#ifdef __RF24L01_TX_TEST__

int main(void)//方向盘ecu
{
    SystemInit();
    USART_Config();

    drv_delay_init();        // 延时定时器初始化
    drv_spi_init();          // SPI 初始化（硬件或软件由宏决定）
    NRF24L01_Gpio_Init();    // RF 引脚初始化
    NRF24L01_check();        // 检测模块是否存在（若不存在会卡死循环）
    RF24L01_Init();          // RF 模块初始化
    RF24L01_Set_Mode(MODE_TX);   // 设置为发送模式

    uint8_t tx_buffer[TX_DATA_LEN];
	
	Wheel_ADC_Init();
	Pedal_ADC_Init();
	
    while (1)
    {
		uint16_t wheel_value=Wheel_GetValue();
        uint8_t pedal_value=Pedal_GetValue();
        uint8_t DNR=Get_DNR();
		uint8_t LNR=Get_LeftRight();
		
		tx_buffer[0]=0x01;
		tx_buffer[1]=wheel_value/256;
		tx_buffer[2]=wheel_value%256;
		tx_buffer[3]=pedal_value;
		tx_buffer[4]=0x02;
		tx_buffer[5]=DNR;
		tx_buffer[6]=0x03;
		tx_buffer[7]=LNR;
		
        // 发送数据包
        uint8_t status = NRF24L01_TxPacket(tx_buffer, TX_DATA_LEN);

		//如果需要可在方向盘端此处修改灵敏度，不建议过高过低
        drv_delay_ms(5);
    }
}

#else   /* 接收端 */ /*需要主要编写的代码段*/

	#ifdef _MAIN_ECU_

	int main(void)//车端
	{
		SystemInit();
		SysTick_Init();
		USART_Config();
		
		drv_delay_init();
		drv_spi_init();
		NRF24L01_Gpio_Init();
		NRF24L01_check();
		RF24L01_Init();
		RF24L01_Set_Mode(MODE_RX);   // 设置为接收模式
		
		uint8_t rx_buffer[32];       // 接收缓冲区（足够大）
		volatile uint8_t data_len;
		
		OLED_Init();
		
		WS2812_Init();
		
		Turn_Init();
		Motor_Init();
		
		Turn_SetDuty(50);    // 开机归中
		Motor1_SetDuty(50);
		Motor2_SetDuty(50);
		//Motor_SetCW(0);
		
		
		while (1)	//	核心的主频只有72MHz，因此需要尽量剪枝掉耗时的语句；禁止超频
		{
			data_len = NRF24L01_RxPacket(rx_buffer);   // 等待并接收数据
			
			/*********************************接收数据处理**************************************/
			/*数据组成：（方向盘转角、踏板标志）（方向盘转角量1）（方向盘转角量2）（油门踏板量）（前进后退空挡标志）（前进后退空挡信号）（左右转向灯标志）（左右转灯信号）*/
			/*标志位用来二次验证，以免数据错用*/
			if(!(rx_buffer[0]==0x01&&rx_buffer[4]==0x02&&rx_buffer[6]==0x03))
				continue;
			
			/*printf("wheel:%2d  pedal:%3d  DNR:%c  light:%s\n" ,
					map_wheel(rx_buffer[1]*256+rx_buffer[2]),
					map_pedal(rx_buffer[3]), 
					rx_buffer[5]==1?'D':(rx_buffer[5]==2?'R':'N'), 
					rx_buffer[7]==1?"left":(rx_buffer[7]==2?"right":"mid"));*///串口通信较慢，仅调试时启用
			
		}
	}
	
	#else
	int main()//测距ecu
	{
		SystemInit();
		SysTick_Init();
		USART_Config();
		uint16_t distance=0;
		while(1)
		{
			distance=Distance_Update(1);
			printf("%d\n",distance);
		}
	}
	#endif

#endif
