// Drivers/ESP8266/esp8266.c
#include "esp8266.h"
#include <string.h>
#include <stdio.h>

static UART_HandleTypeDef* _huart;
static bool tcp_connected = false;

static bool send_cmd(const char* cmd, const char* expect, uint32_t timeout_ms)
{
    HAL_UART_Transmit(_huart, (uint8_t*)cmd, strlen(cmd), 1000);
    uint8_t rx[256] = {0};
    uint32_t t0 = HAL_GetTick();
    uint16_t idx = 0;
    while ((HAL_GetTick() - t0) < timeout_ms && idx < sizeof(rx)-1) {
        uint8_t ch;
        if (HAL_UART_Receive(_huart, &ch, 1, 10) == HAL_OK) {
            rx[idx++] = ch;
            rx[idx] = 0;
            if (strstr((char*)rx, expect) != NULL) return true;
            if (strstr((char*)rx, "ERROR")) return false;
        }
    }
    return strstr((char*)rx, expect) != NULL;
}

bool ESP_Init(UART_HandleTypeDef* huart)
{
    _huart = huart;
    // Basic reset and echo off (optional)
    if (!send_cmd("AT\r\n", "OK", 1000)) return false;
    send_cmd("ATE0\r\n", "OK", 500);
    send_cmd("AT+CIPMUX=0\r\n", "OK", 500);
    return true;
}

bool ESP_SetWiFiMode(ESP_WiFiMode mode)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CWMODE=%d\r\n", mode);
    return send_cmd(cmd, "OK", 1000);
}

bool ESP_ConnectWiFi(const char* ssid, const char* pass)
{
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CWJAP=\"%s\",\"%s\"\r\n", ssid, pass);
    return send_cmd(cmd, "WIFI CONNECTED", 10000);
}

bool ESP_OpenTCP(const char* host, uint16_t port)
{
    char cmd[160];
    snprintf(cmd, sizeof(cmd), "AT+CIPSTART=\"TCP\",\"%s\",%u\r\n", host, port);
    tcp_connected = send_cmd(cmd, "CONNECT", 5000);
    return tcp_connected;
}

bool ESP_ReopenTCP(const char* host, uint16_t port)
{
    send_cmd("AT+CIPCLOSE\r\n", "OK", 1000);
    tcp_connected = false;
    return ESP_OpenTCP(host, port);
}

bool ESP_IsTCPConnected(void)
{
    return tcp_connected;
}

bool ESP_SendTCP(const uint8_t* data, uint16_t len)
{
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%u\r\n", len);
    if (!send_cmd(cmd, ">", 2000)) return false;
    HAL_UART_Transmit(_huart, (uint8_t*)data, len, 5000);
    return send_cmd("", "SEND OK", 2000);
}
