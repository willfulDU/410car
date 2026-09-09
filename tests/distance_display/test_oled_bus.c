#include <stdio.h>
#include <string.h>
#include "oled/oled.h"

extern const uint8_t OLED_F8x16[95][16];
static unsigned transactions, bytes, page, column, bits, position;
static unsigned sda = 1U, scl = 1U, active, control, shift;
static uint8_t pixels[8][128];

void RCC_APB2PeriphClockCmd(int clock, int enabled) { (void)clock; (void)enabled; }
void GPIO_Init(int port, GPIO_InitTypeDef *init) { (void)port; (void)init; }

static void bus_byte(unsigned value)
{
    ++bytes;
    if (position == 1U) { control = value; }
    else if (position >= 2U) {
        if (control == 0x00U) {
            if ((value & 0xF8U) == 0xB0U) { page = value & 7U; }
            else if ((value & 0xF0U) == 0x10U) { column = (column & 15U) | ((value & 15U) << 4); }
            else if ((value & 0xF0U) == 0U) { column = (column & 0xF0U) | value; }
        } else if (control == 0x40U) {
            if (column < 128U) { pixels[page][column] = (uint8_t)value; }
            column = (column + 1U) & 127U;
        }
    }
    ++position;
}

void GPIO_WriteBit(int port, uint16_t pin, int value)
{
    unsigned level = (unsigned)(value != 0);
    (void)port;
    if (pin == GPIO_Pin_7) {
        if (scl && sda && !level) {
            active = 1U; bits = 0U; shift = 0U; position = 0U; ++transactions;
        } else if (scl && !sda && level) { active = 0U; }
        sda = level;
    } else if (pin == GPIO_Pin_6) {
        if (!scl && level && active) {
            if (bits < 8U) { shift = (shift << 1) | sda; }
            if (++bits == 9U) { bus_byte(shift); bits = 0U; shift = 0U; }
        }
        scl = level;
    }
}

static int verify_field(const char *text, unsigned line, unsigned first)
{
    unsigned p, x, i, character, half, index;
    for (p = 0U; p < 8U; ++p) {
        for (x = 0U; x < 128U; ++x) {
            uint8_t expected = 0xA5U;
            if ((p >= (line - 1U) * 2U) && (p < line * 2U) &&
                (x >= (first - 1U) * 8U) && (x < ((first - 1U) + strlen(text)) * 8U)) {
                character = (x - (first - 1U) * 8U) / 8U;
                half = p % 2U;
                i = x % 8U;
                if (text[character] >= '0' && text[character] <= '9') {
                    index = 23U + (unsigned)(text[character] - '0');
                } else if (text[character] == 'D') { index = 36U; }
                else if (text[character] == ':') { index = 16U; }
                else if (text[character] == 'm') { index = 71U; }
                else { index = 0U; }
                expected = OLED_F8x16[index][half * 8U + i];
            }
            if (pixels[p][x] != expected) {
                printf("FAIL pixel page=%u column=%u\n", p, x); return 0;
            }
        }
    }
    return 1;
}

int main(void)
{
    memset(pixels, 0xA5, sizeof(pixels));
    OLED_ShowString(2U, 6U, "D:  356mm");
    printf("Distance field: %u I2C transactions, %u bytes\n", transactions, bytes);
    if (!verify_field("D:  356mm", 2U, 6U)) { return 1; }
    if (transactions > 8U || bytes > 166U) {
        puts("FAIL: distance redraw performs per-byte transactions before the next radio poll"); return 1;
    }
    transactions = 0U;
    memset(pixels, 0xA5, sizeof(pixels));
    OLED_ShowString(2U, 16U, "12");
    if (!verify_field("1", 2U, 16U)) { return 1; }
    OLED_ShowString(0U, 1U, "X");
    OLED_ShowString(5U, 1U, "X");
    OLED_ShowString(1U, 0U, "X");
    OLED_ShowString(1U, 17U, "X");
    if (transactions > 8U) { puts("FAIL: invalid coordinates wrote to OLED"); return 1; }
    puts("PASS: OLED batch budget, glyphs, clipping and unchanged neighboring fields");
    return 0;
}
