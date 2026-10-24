#include "ds18b20.h"

/*
 * DS18B20 DATA pin
 *
 * PC0 is used.
 */

#define DS18B20_PORT GPIOC
#define DS18B20_PIN  GPIO_PIN_0


static void DS18B20_Pin_Output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DS18B20_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}


static void DS18B20_Pin_Input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = DS18B20_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(DS18B20_PORT, &GPIO_InitStruct);
}


/*
 * DS18B20 timing requires microsecond delays.
 *
 * For a first hardware test, this implementation uses
 * the DWT cycle counter.
 */

static void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}


static void delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = us * (HAL_RCC_GetHCLKFreq() / 1000000);

    while ((DWT->CYCCNT - start) < cycles)
    {
    }
}


/*
 * Reset pulse
 */

static uint8_t DS18B20_Reset(void)
{
    uint8_t presence;

    DS18B20_Pin_Output();

    HAL_GPIO_WritePin(DS18B20_PORT,
                      DS18B20_PIN,
                      GPIO_PIN_RESET);

    delay_us(480);

    DS18B20_Pin_Input();

    delay_us(70);

    presence = (HAL_GPIO_ReadPin(DS18B20_PORT,
                                 DS18B20_PIN) == GPIO_PIN_RESET);

    delay_us(410);

    return presence;
}


/*
 * Write one bit
 */

static void DS18B20_WriteBit(uint8_t bit)
{
    DS18B20_Pin_Output();

    HAL_GPIO_WritePin(DS18B20_PORT,
                      DS18B20_PIN,
                      GPIO_PIN_RESET);

    if (bit)
    {
        delay_us(6);

        DS18B20_Pin_Input();

        delay_us(64);
    }
    else
    {
        delay_us(60);

        DS18B20_Pin_Input();

        delay_us(10);
    }
}


/*
 * Read one bit
 */

static uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit;

    DS18B20_Pin_Output();

    HAL_GPIO_WritePin(DS18B20_PORT,
                      DS18B20_PIN,
                      GPIO_PIN_RESET);

    delay_us(6);

    DS18B20_Pin_Input();

    delay_us(9);

    bit = HAL_GPIO_ReadPin(DS18B20_PORT,
                           DS18B20_PIN);

    delay_us(55);

    return bit;
}


/*
 * Write byte
 */

static void DS18B20_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        DS18B20_WriteBit(data & 0x01);

        data >>= 1;
    }
}


/*
 * Read byte
 */

static uint8_t DS18B20_ReadByte(void)
{
    uint8_t data = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (DS18B20_ReadBit())
        {
            data |= (1 << i);
        }
    }

    return data;
}


/*
 * Initialize DS18B20
 */

HAL_StatusTypeDef DS18B20_Init(void)
{
    DWT_Init();

    if (!DS18B20_Reset())
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}


/*
 * Read temperature
 */

HAL_StatusTypeDef DS18B20_ReadTemperature(float *temperature)
{
    uint8_t temp_l;
    uint8_t temp_h;

    int16_t raw_temp;

    /*
     * Reset
     */

    if (!DS18B20_Reset())
    {
        return HAL_ERROR;
    }

    /*
     * Skip ROM
     */

    DS18B20_WriteByte(0xCC);

    /*
     * Start temperature conversion
     */

    DS18B20_WriteByte(0x44);

    /*
     * Wait for conversion.
     *
     * 750 ms is safe for 12-bit conversion.
     */

    HAL_Delay(750);

    /*
     * Read scratchpad
     */

    if (!DS18B20_Reset())
    {
        return HAL_ERROR;
    }

    DS18B20_WriteByte(0xCC);

    DS18B20_WriteByte(0xBE);

    temp_l = DS18B20_ReadByte();
    temp_h = DS18B20_ReadByte();

    raw_temp = ((int16_t)temp_h << 8) | temp_l;

    *temperature = raw_temp / 16.0f;

    return HAL_OK;
}
