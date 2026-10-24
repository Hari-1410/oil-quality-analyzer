#include "lcd.h"
#include <string.h>

#define LCD_I2C_ADDR (0x27 << 1)

#define LCD_RS 0x01
#define LCD_RW 0x02
#define LCD_EN 0x04
#define LCD_BL 0x08

static I2C_HandleTypeDef *lcd_i2c;


static void LCD_Write4Bits(uint8_t data)
{
    uint8_t value;

    value = data | LCD_BL;

    HAL_I2C_Master_Transmit(lcd_i2c,
                            LCD_I2C_ADDR,
                            &value,
                            1,
                            HAL_MAX_DELAY);

    value = data | LCD_BL | LCD_EN;

    HAL_I2C_Master_Transmit(lcd_i2c,
                            LCD_I2C_ADDR,
                            &value,
                            1,
                            HAL_MAX_DELAY);

    HAL_Delay(1);

    value = data | LCD_BL;

    HAL_I2C_Master_Transmit(lcd_i2c,
                            LCD_I2C_ADDR,
                            &value,
                            1,
                            HAL_MAX_DELAY);

    HAL_Delay(1);
}


static void LCD_SendCommand(uint8_t cmd)
{
    uint8_t high = cmd & 0xF0;
    uint8_t low  = (cmd << 4) & 0xF0;

    LCD_Write4Bits(high);
    LCD_Write4Bits(low);
}


static void LCD_SendData(uint8_t data)
{
    uint8_t high = (data & 0xF0) | LCD_RS;
    uint8_t low  = ((data << 4) & 0xF0) | LCD_RS;

    LCD_Write4Bits(high);
    LCD_Write4Bits(low);
}


void LCD_Init(I2C_HandleTypeDef *hi2c)
{
    lcd_i2c = hi2c;

    HAL_Delay(50);

    LCD_Write4Bits(0x30);
    HAL_Delay(5);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x30);
    HAL_Delay(1);

    LCD_Write4Bits(0x20);
    HAL_Delay(1);

    LCD_SendCommand(0x28);   // 4-bit, 2-line, 5x8 font
    LCD_SendCommand(0x08);   // Display OFF
    LCD_SendCommand(0x01);   // Clear display
    HAL_Delay(2);

    LCD_SendCommand(0x06);   // Entry mode
    LCD_SendCommand(0x0C);   // Display ON, cursor OFF
}


void LCD_Clear(void)
{
    LCD_SendCommand(0x01);
    HAL_Delay(2);
}


void LCD_SetCursor(uint8_t row, uint8_t col)
{
    uint8_t address;

    if (row == 0)
        address = 0x80 + col;
    else
        address = 0xC0 + col;

    LCD_SendCommand(address);
}


void LCD_Print(char *str)
{
    while (*str)
    {
        LCD_SendData((uint8_t)*str);
        str++;
    }
}
