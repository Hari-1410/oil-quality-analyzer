#include "turbidity.h"

extern ADC_HandleTypeDef hadc1;


/*
 * STM32 ADC:
 *
 * 12-bit
 * 0     = 0 V
 * 4095  = 3.3 V
 *
 * Because we use a 20k/10k divider:
 *
 * Vadc = Vsensor / 3
 *
 * Therefore:
 *
 * Vsensor = Vadc * 3
 */


uint32_t Turbidity_ReadRaw(void)
{
    uint32_t adc_value = 0;

    HAL_ADC_Start(&hadc1);

    if (HAL_ADC_PollForConversion(&hadc1, 100) == HAL_OK)
    {
        adc_value = HAL_ADC_GetValue(&hadc1);
    }

    HAL_ADC_Stop(&hadc1);

    return adc_value;
}


float Turbidity_ReadVoltage(void)
{
    uint32_t raw;

    float adc_voltage;
    float sensor_voltage;

    raw = Turbidity_ReadRaw();

    /*
     * ADC voltage
     */

    adc_voltage = ((float)raw * 3.3f) / 4095.0f;

    /*
     * Reverse the 1/3 divider
     */

    sensor_voltage = adc_voltage * 3.0f;

    return sensor_voltage;
}
