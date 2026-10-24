#ifndef DS18B20_H
#define DS18B20_H

#include "main.h"

HAL_StatusTypeDef DS18B20_Init(void);
HAL_StatusTypeDef DS18B20_ReadTemperature(float *temperature);

#endif
