#include "pedal.h"

void Pedal_ADC_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    /* 1. 使能 GPIOA 和 ADC1 时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_ADC1, ENABLE);

    /* 2. ADC 时钟配置：72MHz / 6 = 12MHz */
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    /* 3. PA1 配置为模拟输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* 4. ADC 配置 */
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;
    ADC_InitStructure.ADC_ScanConvMode = DISABLE;
    ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
    ADC_InitStructure.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &ADC_InitStructure);

    /* 5. 先默认配置一次通道1：PA1 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    /* 6. 使能 ADC */
    ADC_Cmd(ADC1, ENABLE);

    /* 7. 复位校准 */
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1) == SET);

    /* 8. 开始校准 */
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1) == SET);
}

uint16_t Pedal_ADC_ReadRaw(void)
{
    /* 每次读取前重新指定通道1，避免与 wheel 共用 ADC1 时串通道 */
    ADC_RegularChannelConfig(ADC1, ADC_Channel_1, 1, ADC_SampleTime_239Cycles5);

    /* 启动一次转换 */
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);

    /* 等待转换完成 */
    while (ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC) == RESET);

    /* 返回 ADC 原始值 0~4095 */
    return ADC_GetConversionValue(ADC1);
}

float Pedal_GetVoltage(void)
{
    uint16_t adc_value;
    adc_value = Pedal_ADC_ReadRaw();

    /* 转换为电压值 0~3.3V */
    return ((float)adc_value * 3.3f) / 4095.0f;
}

uint8_t Pedal_GetValue(void)
{
    uint16_t adc_value;
    uint32_t temp;

    adc_value = Pedal_ADC_ReadRaw();
    temp = (uint32_t)adc_value * 255 + 2047;
    return (uint8_t)(temp / 4095);
}

uint16_t Pedal_ADC_ReadAverage(uint8_t times)
{
    uint32_t sum = 0;
    uint8_t i;

    for (i = 0; i < times; i++)
    {
        sum += Pedal_ADC_ReadRaw();
    }

    return (uint16_t)(sum / times);
}

uint8_t Pedal_GetValue255_Avg(uint8_t times)
{
    uint16_t adc_value;
    uint32_t temp;

    adc_value = Pedal_ADC_ReadAverage(times);
    temp = (uint32_t)adc_value * 255 + 2047;
    return (uint8_t)(temp / 4095);
}
