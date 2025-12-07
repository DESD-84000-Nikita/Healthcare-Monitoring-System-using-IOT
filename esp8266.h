// Drivers/ESP8266/esp8266.h
#ifndef ESP8266_H
#define ESP8266_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum { ESP_MODE_STA = 1 } ESP_WiFiMode;

bool ESP_Init(UART_HandleTypeDef* huart);
bool ESP_SetWiFiMode(ESP_WiFiMode mode);
bool ESP_ConnectWiFi(const char* ssid, const char* pass);
bool ESP_OpenTCP(const char* host, uint16_t port);
bool ESP_ReopenTCP(const char* host, uint16_t port);
bool ESP_IsTCPConnected(void);
bool ESP_SendTCP(const uint8_t* data, uint16_t len);

#endif
