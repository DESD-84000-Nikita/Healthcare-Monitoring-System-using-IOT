// Drivers/MAX30100/max30100.h
#ifndef MAX30100_H
#define MAX30100_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define MAX30100_I2C_ADDR 0x57 << 1

typedef enum {
    MAX30100_OK = 0,
    MAX30100_ERR
} MAX30100_Status;

typedef enum {
    MAX30100_MODE_HR = 0x02,
    MAX30100_MODE_SPO2 = 0x03,
    MAX30100_MODE_SPO2_HR = 0x03
} MAX30100_Mode;

typedef enum { RED_LED_27_1mA = 0x0F, IR_LED_50mA = 0x3F } MAX30100_LEDCurrent;

typedef enum {
    SPO2_SAMPLE_RATE_100HZ = 0x04 << 2,
    SPO2_ADC_RANGE_2048    = 0x00 << 5,
    SPO2_LED_PW_1600us     = 0x03
} MAX30100_SPO2Config;

MAX30100_Status MAX30100_Init(I2C_HandleTypeDef* hi2c);
void MAX30100_SetMode(MAX30100_Mode mode);
void MAX30100_SetLEDs(MAX30100_LEDCurrent red, MAX30100_LEDCurrent ir);
void MAX30100_SetSpO2Config(uint8_t sample_rate, uint8_t adc_range, uint8_t led_pw);
void MAX30100_Update(void);
void MAX30100_Compute(float* spo2, float* hr);

#endif
