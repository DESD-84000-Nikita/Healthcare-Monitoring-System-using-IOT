// Drivers/DS18B20/ds18b20.h
#ifndef DS18B20_H
#define DS18B20_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>

void DS18B20_Init(GPIO_TypeDef* port, uint16_t pin);
bool DS18B20_Reset(void);
void DS18B20_WriteByte(uint8_t byte);
uint8_t DS18B20_ReadByte(void);
bool DS18B20_StartConversion(void);
bool DS18B20_ReadTemperature(float* temp_c);

#endif
