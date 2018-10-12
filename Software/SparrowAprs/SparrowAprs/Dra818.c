#include <stm32f4xx_hal.h>
#include <stdio.h>
#include <string.h>
#include "Watchdog.h"

static UART_HandleTypeDef UartHandle;

static uint8_t xBuffer[200];

#define MAX_TIMEOUT				2000
#define DRA818_CONNECT_LEN		15
#define DRA818_GROUP_LEN		48
#define DRA818_FILTER_LEN		20

// PA2/PA3
void Dra818Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	// Enable clocks
	__GPIOA_CLK_ENABLE();
	__USART2_CLK_ENABLE();
	
	// Configure GPIO
	GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull      = GPIO_NOPULL;
	GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
	GPIO_InitStruct.Alternate = GPIO_AF7_USART2;

	// TX
	GPIO_InitStruct.Pin       = GPIO_PIN_2;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	// RX
	GPIO_InitStruct.Pin       = GPIO_PIN_3;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
	
	// Configure the USART peripheral
	UartHandle.Instance          = USART2;
	UartHandle.Init.BaudRate     = 9600;
	UartHandle.Init.WordLength   = UART_WORDLENGTH_8B;
	UartHandle.Init.StopBits     = UART_STOPBITS_1;
	UartHandle.Init.Parity       = UART_PARITY_NONE;
	UartHandle.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
	UartHandle.Init.Mode         = UART_MODE_TX_RX;

	// Commit the USART
	if (HAL_UART_Init(&UartHandle) != HAL_OK)
	{
		while (1) ;
	}
}

uint8_t Dra818Connect(void)
{
	char command[DRA818_CONNECT_LEN] = "AT+DMOCONNECT\r\n";
	HAL_UART_Transmit(&UartHandle, (uint8_t*)command, strlen(command), MAX_TIMEOUT);
	return 1;
}

uint8_t Dra818SetGroup(const float txFreq, const float rxFreq, const char ctcssTx[4], const char ctcssRx[4], const uint8_t squelch)
{
	sprintf((char*)&xBuffer, "AT+DMOSETGROUP=0,%03.4f,%03.4f,%.4s,%c,%.4s\r\n", txFreq, rxFreq, ctcssTx, squelch + '0', ctcssRx);
	HAL_UART_Transmit(&UartHandle, xBuffer, DRA818_GROUP_LEN, MAX_TIMEOUT);
	return 1;
}

uint8_t Dra818SetFilter(const uint8_t predeemphasis, const uint8_t highpass, const uint8_t lowpass)
{
	sprintf((char*)xBuffer, "AT+SETFILTER=%c,%c,%c\r\n", predeemphasis + '0', highpass + '0', lowpass + '0');
	HAL_UART_Transmit(&UartHandle, xBuffer, DRA818_FILTER_LEN, MAX_TIMEOUT);
	return 1;
}