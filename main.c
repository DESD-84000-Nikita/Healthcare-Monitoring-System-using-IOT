// Core/Src/main.c
#include "stm32f4xx_hal.h"
#include "max30100.h"
#include "ds18b20.h"
#include "esp8266.h"
#include <stdio.h>
#include <string.h>

I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);

static void publish_to_cloud(float temperature_c, float spo2, float heart_rate);

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();

    // Initialize sensors and ESP
    if (MAX30100_Init(&hi2c1) != MAX30100_OK) {
        // Handle error (blink LED or log)
    }
    MAX30100_SetMode(MAX30100_MODE_SPO2_HR);
    MAX30100_SetLEDs(RED_LED_27_1mA, IR_LED_50mA);
    MAX30100_SetSpO2Config(SPO2_SAMPLE_RATE_100HZ, SPO2_ADC_RANGE_2048, SPO2_LED_PW_1600us);

    DS18B20_Init(GPIOA, GPIO_PIN_1);

    // Connect Wi-Fi and prepare HTTP endpoint
    if (!ESP_Init(&huart1)) {
        // Handle ESP init error
    }
    ESP_SetWiFiMode(ESP_MODE_STA);
    ESP_ConnectWiFi("YOUR_SSID", "YOUR_PASSWORD");
    // Option 1: direct TCP to your server; Option 2: simple HTTP POST via AT+CIPSTART/CIPSEND
    // Configure server details
    const char* host = "example.com";
    const uint16_t port = 80;
    ESP_OpenTCP(host, port);

    float tempC = 0.0f;
    float spo2 = 0.0f;
    float hr = 0.0f;

    // Main loop: sample, compute, publish
    while (1)
    {
        // DS18B20 conversion (750ms max for 12-bit)
        if (DS18B20_StartConversion()) {
            HAL_Delay(750);
            DS18B20_ReadTemperature(&tempC);
        }

        // MAX30100 sampling
        MAX30100_Update(); // reads FIFO into internal buffer
        MAX30100_Compute(&spo2, &hr); // simple algorithm (demo)

        // Publish to cloud every 2 seconds
        publish_to_cloud(tempC, spo2, hr);
        HAL_Delay(2000);
    }
}

static void publish_to_cloud(float temperature_c, float spo2, float heart_rate)
{
    char payload[256];
    // Basic JSON; adjust endpoint path and body as needed
    // Example HTTP POST to /api/health
    snprintf(payload, sizeof(payload),
             "POST /api/health HTTP/1.1\r\n"
             "Host: example.com\r\n"
             "Content-Type: application/json\r\n"
             "Connection: keep-alive\r\n"
             "Content-Length: %d\r\n\r\n"
             "{\"device\":\"stm32f407\",\"temp\":%.2f,\"spo2\":%.2f,\"hr\":%.2f}",
             60, temperature_c, spo2, heart_rate);

    if (!ESP_IsTCPConnected()) {
        ESP_ReopenTCP("example.com", 80);
    }
    ESP_SendTCP((uint8_t*)payload, strlen(payload));
}

static void MX_I2C1_Init(void)
{
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 400000; // Fast mode for MAX30100
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if (HAL_I2C_Init(&hi2c1) != HAL_OK) {
        // Error handler
    }
}

static void MX_USART1_UART_Init(void)
{
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        // Error handler
    }
}

static void MX_GPIO_Init(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // DS18B20 pin PA1: open-drain output, pull-up external
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Optional: MAX30100 INT pin, ESP EN, LED, etc.
}

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 336;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {}

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|
                                  RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {}
}
