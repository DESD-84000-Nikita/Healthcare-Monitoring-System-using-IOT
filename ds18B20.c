// Drivers/DS18B20/ds18b20.c
#include "ds18b20.h"

static GPIO_TypeDef* _port;
static uint16_t _pin;

static void set_output(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = _pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(_port, &GPIO_InitStruct);
}

static void set_input(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = _pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(_port, &GPIO_InitStruct);
}

static void write_low(void) { HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_RESET); }
static void write_high(void){ HAL_GPIO_WritePin(_port, _pin, GPIO_PIN_SET); }
static uint8_t read_pin(void){ return HAL_GPIO_ReadPin(_port, _pin); }

static void delay_us(uint16_t us)
{
    // Simple busy-wait; for accuracy, use TIM-based microsecond delay
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks) {}
}

void DS18B20_Init(GPIO_TypeDef* port, uint16_t pin)
{
    _port = port; _pin = pin;

    // Enable DWT cycle counter for delay_us
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    set_output();
    write_high(); // idle high
}

bool DS18B20_Reset(void)
{
    set_output();
    write_low();
    delay_us(480);
    write_high();
    delay_us(70);
    set_input();
    uint8_t presence = !read_pin(); // presence pulse is low
    delay_us(410);
    set_output();
    write_high();
    return presence;
}

static void write_bit(uint8_t bit)
{
    set_output();
    write_low();
    if (bit) {
        delay_us(6);
        write_high();
        delay_us(64);
    } else {
        delay_us(60);
        write_high();
        delay_us(10);
    }
}

static uint8_t read_bit(void)
{
    uint8_t bit;
    set_output();
    write_low();
    delay_us(6);
    set_input();
    delay_us(9);
    bit = read_pin();
    delay_us(55);
    set_output();
    write_high();
    return bit;
}

void DS18B20_WriteByte(uint8_t byte)
{
    for (int i = 0; i < 8; ++i) {
        write_bit(byte & 0x01);
        byte >>= 1;
    }
}

uint8_t DS18B20_ReadByte(void)
{
    uint8_t value = 0;
    for (int i = 0; i < 8; ++i) {
        value >>= 1;
        if (read_bit()) value |= 0x80;
    }
    return value;
}

bool DS18B20_StartConversion(void)
{
    if (!DS18B20_Reset()) return false;
    DS18B20_WriteByte(0xCC); // Skip ROM
    DS18B20_WriteByte(0x44); // Convert T
    return true;
}

bool DS18B20_ReadTemperature(float* temp_c)
{
    if (!DS18B20_Reset()) return false;
    DS18B20_WriteByte(0xCC); // Skip ROM
    DS18B20_WriteByte(0xBE); // Read Scratchpad
    uint8_t temp_lsb = DS18B20_ReadByte();
    uint8_t temp_msb = DS18B20_ReadByte();
    int16_t raw = (int16_t)((temp_msb << 8) | temp_lsb);
    *temp_c = raw / 16.0f; // 12-bit resolution
    return true;
}
