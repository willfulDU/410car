#include "ws2812b.h"

#define BYTES_PER_LED  15
#define RESET_BYTES    60

static uint8_t led_data[LED_NUM][3];
static uint8_t spi_buf_all[LED_NUM * BYTES_PER_LED + RESET_BYTES];

static void delay_us(uint32_t us)
{
    volatile uint32_t i;
    while (us--)
    {
        for (i = 0; i < 60; i++) { __NOP(); }
    }
}

static void pin_af(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin = GPIO_Pin_15;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOB, &g);
}

static void pin_low(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin = GPIO_Pin_15;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(GPIOB, &g);
    GPIO_ResetBits(GPIOB, GPIO_Pin_15);
}

static void spi_init(void)
{
    SPI_InitTypeDef s;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_SPI2, ENABLE);
    pin_af();
    SPI_I2S_DeInit(SPI2);
    s.SPI_Direction = SPI_Direction_1Line_Tx;
    s.SPI_Mode = SPI_Mode_Master;
    s.SPI_DataSize = SPI_DataSize_8b;
    s.SPI_CPOL = SPI_CPOL_Low;
    s.SPI_CPHA = SPI_CPHA_1Edge;
    s.SPI_NSS = SPI_NSS_Soft;
    s.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;
    s.SPI_FirstBit = SPI_FirstBit_MSB;
    s.SPI_CRCPolynomial = 7;
    SPI_Init(SPI2, &s);
    SPI_Cmd(SPI2, ENABLE);
}

static void spi_send(uint8_t d)
{
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    SPI_I2S_SendData(SPI2, d);
}

static void encode_byte(uint8_t byte, uint8_t *buf)
{
    uint64_t code = 0;
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        code <<= 5;
        if (byte & 0x80) code |= 0x1C;
        else             code |= 0x18;
        byte <<= 1;
    }
    buf[0] = (uint8_t)(code >> 32);
    buf[1] = (uint8_t)(code >> 24);
    buf[2] = (uint8_t)(code >> 16);
    buf[3] = (uint8_t)(code >> 8);
    buf[4] = (uint8_t)(code);
}

void WS2812_Init(void)
{
    spi_init();
    WS2812_Clear();   // 清空所有灯缓存
    WS2812_Show();    // 上电立即熄灭所有灯，避免残留随机颜色
}

void WS2812_SetPixel(uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{
    if (n >= LED_NUM) return;
    led_data[n][0] = g;
    led_data[n][1] = r;
    led_data[n][2] = b;
}

void WS2812_Clear(void)
{
    uint16_t i;
    for (i = 0; i < LED_NUM; i++)
        led_data[i][0] = led_data[i][1] = led_data[i][2] = 0;
}

void WS2812_Show(void)
{
    uint16_t i, idx = 0;
    uint16_t total = LED_NUM * BYTES_PER_LED + RESET_BYTES;

    for (i = 0; i < LED_NUM; i++)
    {
        encode_byte(led_data[i][0], &spi_buf_all[idx]); idx += 5;
        encode_byte(led_data[i][1], &spi_buf_all[idx]); idx += 5;
        encode_byte(led_data[i][2], &spi_buf_all[idx]); idx += 5;
    }
    while (idx < total) spi_buf_all[idx++] = 0x00;

    /* 临界区：发送期间禁止中断，避免 SysTick(10us) 打断 SPI 时序
     * 导致 WS2812B 数据错位、颜色乱码 */
    __disable_irq();


    /* 阶段1: 复位 */
    pin_low();
    delay_us(80);

    /* 阶段2: 开 SPI + 发 24bit 前导 (3 × 0xAA)
       每个 0xAA = 10101010, 共 24 个独立脉冲
       第一个灯完整吃掉这 24bit, 显示为全灭
       SPI 初始化的毛刺也在这个前导中被吸收 */
    pin_af();
    SPI_Cmd(SPI2, ENABLE);
    spi_send(0xAA);
    spi_send(0xAA);
    spi_send(0xAA);

    /* 阶段3: 复位 >107μs → 第一个灯丢弃前导数据 */
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);
    for (i = 0; i < 60; i++)
        spi_send(0x00);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);

    /* 阶段4: 发真实 20 组数据 (SPI/引脚无切换) */
    for (i = 0; i < total; i++)
        spi_send(spi_buf_all[i]);

    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_TXE) == RESET);
    while (SPI_I2S_GetFlagStatus(SPI2, SPI_I2S_FLAG_BSY) == SET);

    SPI_Cmd(SPI2, DISABLE);
    pin_low();
    delay_us(80);

    __enable_irq();   // 恢复中断
}
void WS2812_Blink(uint8_t left, uint8_t right,
                  uint8_t r, uint8_t g, uint8_t b,
                  uint16_t delay)
{
    static uint16_t cnt = 0;
    static uint8_t blink_state = 0;

    static uint8_t last_left  = 0xFF;
    static uint8_t last_right = 0xFF;
    static uint8_t last_r     = 0xFF;
    static uint8_t last_g     = 0xFF;
    static uint8_t last_b     = 0xFF;
    static uint8_t last_state = 0xFF;

    if (delay == 0)
    {
        delay = 1;
    }

    cnt++;

    if (cnt >= delay)
    {
        cnt = 0;
        blink_state = !blink_state;
    }

    if ((last_left  == left)       &&
        (last_right == right)      &&
        (last_r     == r)          &&
        (last_g     == g)          &&
        (last_b     == b)          &&
        (last_state == blink_state))
    {
        return;
    }

    last_left  = left;
    last_right = right;
    last_r     = r;
    last_g     = g;
    last_b     = b;
    last_state = blink_state;

    /* 先关闭所有灯，防止之前的灯状态残留 */
    WS2812_Clear();   // 清空所有灯（0~LED_NUM-1），避免灯 0 残留

    /* 闪烁亮起阶段 */
    if (blink_state)
    {
        if (left && right)
        {
            /* left 和 right 同时为 1：3、4 号灯闪红灯 */
            WS2812_SetPixel(3, 255, 0, 0);
            WS2812_SetPixel(4, 255, 0, 0);
        }
        else if (left)
        {
            /* 仅左侧有效：1、3 号灯 */
            WS2812_SetPixel(1, r, g, b);
            WS2812_SetPixel(3, r, g, b);
        }
        else if (right)
        {
            /* 仅右侧有效：2、4 号灯 */
            WS2812_SetPixel(2, r, g, b);
            WS2812_SetPixel(4, r, g, b);
        }
    }

    WS2812_Show();
}
