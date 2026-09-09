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
#include "switch.h"
#include "wheel_speed.h"

#endif

#define TX_DATA_LEN  8   // 每帧发送 8 个字节（即 8 个两位十六进制数）
#define GROUP_NO     "01"       // 组号（上电/重置后显示）
#define SHOW_DATE    "2026.9.9" // 显示日期
#define PINK_R  255   // 转向灯颜色：粉色（R）
#define PINK_G  105   // 转向灯颜色：粉色（G）
#define PINK_B  180   // 转向灯颜色：粉色（B）

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
	Switch_Init();
	
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

	#define WHEEL_SPEED_DISPLAY_PERIOD_MS  250U

	static void WheelSpeed_DisplayUpdate(void)
	{
		static uint32_t last_update_ms = 0U;
		static char previous[14] = "";
		char text[14];
		uint32_t now;

		now = SysTick_GetTick();
		if ((uint32_t)(now - last_update_ms) < WHEEL_SPEED_DISPLAY_PERIOD_MS)
		{
			return;
		}
		last_update_ms = now;

		strcpy(text, "L:     R:    ");
		WheelSpeedCalc_Format(text + 2, WheelSpeed_GetRpm(WHEEL_LEFT));
		text[6] = ' ';
		WheelSpeedCalc_Format(text + 9, WheelSpeed_GetRpm(WHEEL_RIGHT));

		if (strcmp(text, previous) != 0)
		{
			OLED_ShowString(4, 1, text);
			strcpy(previous, text);
		}
	}

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
		uint8_t last_dnr = 0xFF;
		uint8_t last_left_right = 0xFF;
		uint16_t wheel_value;
		uint8_t motor_duty;
		uint8_t direction;
		static uint8_t blink_on = 0;      // 转向灯闪烁相位（0=灭，1=亮）
		
		OLED_Init();
		/* 静态区：2x 组号占第 1~2 行第 1~4 列，日期在其右侧。 */
		OLED_ShowString2x(1, 1, GROUP_NO);
		OLED_ShowString(1, 6, SHOW_DATE);
		WS2812_Init();
		
		Turn_Init();
		Motor_Init();
		WheelSpeed_Init();
		
		Turn_SetDuty(50);    // 开机归中
		Motor_SetCW(0);
		Motor1_SetDuty(0);
		Motor2_SetDuty(0);
		
		
		while (1)	//	核心的主频只有72MHz，因此需要尽量剪枝掉耗时的语句；禁止超频
		{
			data_len = NRF24L01_RxPacket(rx_buffer);   // 等待并接收数据
			WheelSpeed_Update();
			WheelSpeed_DisplayUpdate();
			if (data_len != TX_DATA_LEN)
			{
				Motor_SetCW(0);
				continue;
			}
			
			/*********************************接收数据处理**************************************/
			/*数据组成：（方向盘转角、踏板标志）（方向盘转角量1）（方向盘转角量2）（油门踏板量）（前进后退空挡标志）（前进后退空挡信号）（左右转向灯标志）（左右转灯信号）*/
			/*标志位用来二次验证，以免数据错用*/
			if (data_len < TX_DATA_LEN ||
				!(rx_buffer[0] == 0x01 && rx_buffer[4] == 0x02 && rx_buffer[6] == 0x03))
			{
				Motor_SetCW(0);
				continue;
			}

			if (rx_buffer[5] != last_dnr)
			{
				last_dnr = rx_buffer[5];
				switch (last_dnr)
				{
					case DNR_FORWARD:
						OLED_ShowString(3, 4, "D  ");
						break;
					case DNR_REVERSE:
						OLED_ShowString(3, 4, "R  ");
						break;
					default:
						OLED_ShowString(3, 4, "N  ");
						break;
				}
			}

			if (rx_buffer[7] != last_left_right)
			{
				last_left_right = rx_buffer[7];
				switch (last_left_right)
				{
					case LIGHT_LEFT:
						OLED_ShowString(3, 9, "LEFT ");
						break;
					case LIGHT_RIGHT:
						OLED_ShowString(3, 9, "RIGHT");
						break;
					default:
						OLED_ShowString(3, 9, "OFF  ");
						break;
				}
			}

			wheel_value = ((uint16_t)rx_buffer[1] << 8) | rx_buffer[2];
			Turn_SetDuty(map_wheel(wheel_value));

			direction = rx_buffer[5];
			if ((direction == 1) || (direction == 2))
			{
				motor_duty = map_pedal(rx_buffer[3]);
				Motor_SetCW(direction);
				Motor1_SetDuty(motor_duty);
				Motor2_SetDuty(motor_duty);
			}
			else
			{
				Motor_SetCW(0);
			}
			
			/* ===== 灯光：显式逐灯硬性设置，避免残留/冲突 =====
			 * 先硬性熄灭所有灯，再只点亮该亮的灯；
			 * 倒挡灯常亮红色，转向灯粉色 1s 闪烁（blink_on 每 1s 切换） */
			if (SysTick_GetFlag())
			{
				SysTick_ClearFlag();                          // 清 1s 标志
				blink_on = !blink_on;                         // 每 1s 切换闪烁相位
				
				/* 1. 硬性熄灭所有灯（0~4），杜绝残留 */
				WS2812_SetPixel(0, 0, 0, 0);
				WS2812_SetPixel(1, 0, 0, 0);
				WS2812_SetPixel(2, 0, 0, 0);
				WS2812_SetPixel(3, 0, 0, 0);
				WS2812_SetPixel(4, 0, 0, 0);
				
				/* 2. 点亮该亮的灯 */
				if (rx_buffer[5] == 2)                        // 倒挡：常亮红色（3、4 号）
				{
					WS2812_SetPixel(3, 255, 0, 0);
					WS2812_SetPixel(4, 255, 0, 0);
				}
				else if (blink_on)                            // 转向灯闪烁亮阶段
				{
					if (rx_buffer[7] == 1)                       // 左转向：粉色（1、3 号）
					{
						WS2812_SetPixel(1, PINK_R, PINK_G, PINK_B);
						WS2812_SetPixel(3, PINK_R, PINK_G, PINK_B);
					}
					else if (rx_buffer[7] == 2)                  // 右转向：粉色（2、4 号）
					{
						WS2812_SetPixel(2, PINK_R, PINK_G, PINK_B);
						WS2812_SetPixel(4, PINK_R, PINK_G, PINK_B);
					}
				}
				/* 3. 无转向/无倒挡，或闪烁灭阶段：所有灯已硬性熄灭 */
				
				WS2812_Show();                                // 每次刷新发送一次
			}
			
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
